#!/usr/bin/env python3
"""
Policy execution for ABS (Abstraction-Based Synthesis).

Reads a PDDL domain/problem, an .abs feature definition file, and a .policy
file produced by BQS, then executes the policy from the initial state until a
goal state is reached.

Usage:
    python3 execute_policy.py <domain.pddl> <problem.pddl> <file.abs> <file.policy>

Example:
    python3 execute_policy.py \\
        domains/Gripper-Sim/domain.pddl \\
        domains/Gripper-Sim/prob1-1.pddl \\
        generation/Gripper-Sim/prob1-1.abs \\
        generation/Gripper-Sim/prob1-1.policy
"""

import re
import sys
from dataclasses import dataclass
from itertools import product
from typing import Optional

from pddl import parse_domain, parse_problem
from pddl.logic.base import And, Not
from pddl.logic.predicates import Predicate

# ---------------------------------------------------------------------------
# State representation
# ---------------------------------------------------------------------------

# A ground atom is a tuple: (predicate_name, arg0, arg1, ...)
# A state is a frozenset of ground atoms.

def _atom_to_tuple(atom) -> tuple:
    """Convert a ground pddl Predicate to a plain tuple."""
    return (atom.name,) + tuple(term.name for term in atom.terms)


def _iter_conjuncts(formula) -> tuple:
    if isinstance(formula, And):
        return tuple(formula.operands)
    return (formula,)


def _ground_formula_atom(atom: Predicate, binding: dict[str, str]) -> tuple:
    grounded_args = []
    for term in atom.terms:
        grounded_args.append(binding.get(term.name, term.name))
    return (atom.name,) + tuple(grounded_args)


def _type_name(type_tags: frozenset[str]) -> Optional[str]:
    return next(iter(type_tags), None)


def _goal_predicates(goal) -> frozenset[Predicate]:
    predicates = []
    for clause in _iter_conjuncts(goal):
        if isinstance(clause, Predicate):
            predicates.append(clause)
    return frozenset(predicates)


def _make_initial_state(prob) -> frozenset:
    return frozenset(_atom_to_tuple(a) for a in prob.init)


def _goal_atoms(prob) -> frozenset:
    return frozenset(_atom_to_tuple(a) for a in _goal_predicates(prob.goal))


# ---------------------------------------------------------------------------
# Abs-file parsing
# ---------------------------------------------------------------------------

@dataclass
class NumericFeature:
    """
    Represents a numeric (counting) feature.

    Example:
        N2 = ( { st1(ball) st2(gripper) } , { carry(ball, gripper) } )

    Attributes:
        name:       Feature name, e.g. "N2".
        base_types: Ordered list of base-type names used as variables,
                    e.g. ["ball", "gripper"].
        pred_name:  Predicate name in the pattern, e.g. "carry".
        pred_args:  Argument list where each entry is either ("var", type_name)
                    or ("const", constant_name), e.g.
                    [("var","ball"), ("var","gripper")].
    """
    name: str
    base_types: list[str]          # types acting as variables (in order of the left set)
    pred_name: str
    pred_args: list[tuple[str, str]]  # ("var", type) or ("const", name)


@dataclass
class BooleanFeature:
    """
    Represents a boolean (propositional) feature.

    Example:
        B0 = at-robby(rooma)

    Attributes:
        name:     Feature name, e.g. "B0".
        atom:     Ground atom tuple, e.g. ("at-robby", "rooma").
    """
    name: str
    atom: tuple


def _parse_abs(path: str) -> tuple[dict, dict, dict]:
    """
    Parse a .abs file.

    Returns:
        subtypes:  {subtype_label -> {"base_type": str, "objects": [str]}}
                   e.g. {"st1": {"base_type": "ball", "objects": ["ball1",...]}}
        num_feats: {name -> NumericFeature}
        bool_feats:{name -> BooleanFeature}
    """
    with open(path) as f:
        text = f.read()

    subtypes: dict[str, dict] = {}
    num_feats: dict[str, NumericFeature] = {}
    bool_feats: dict[str, BooleanFeature] = {}

    lines = text.splitlines()
    section = None

    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("==="):
            continue
        # Section headers
        if stripped == "Subtypes:":
            section = "subtypes"
            continue
        if stripped == "Feature Mapping:":
            section = "features"
            continue

        if section == "subtypes":
            # "Subtypes of ball:" — just a heading, skip
            if stripped.startswith("Subtypes of"):
                continue
            # "st1(ball) = { ball1 ball2 ball3 ball4 ball5 }"
            m = re.match(r'(st\d+)\((\w+)\)\s*=\s*\{([^}]*)\}', stripped)
            if m:
                label, base_type, objects_str = m.groups()
                objects = objects_str.split()
                subtypes[label] = {"base_type": base_type, "objects": objects}
            continue

        if section == "features":
            if not stripped or "=" not in stripped:
                continue

            feat_name, _, rest = stripped.partition("=")
            feat_name = feat_name.strip()
            rest = rest.strip()

            if feat_name.startswith("N"):
                # Numeric feature:
                # ( { st1(ball) st2(gripper) } , { carry(ball, gripper) } )
                m = re.match(r'\(\s*\{([^}]*)\}\s*,\s*\{([^}]*)\}\s*\)', rest)
                if not m:
                    continue
                type_set_str, pred_str = m.group(1).strip(), m.group(2).strip()

                # Collect base types (in appearance order, preserving duplicates removed)
                base_types: list[str] = []
                seen: set[str] = set()
                for st_ref in type_set_str.split():
                    # st_ref is like "st1(ball)"
                    tm = re.match(r'st\d+\((\w+)\)', st_ref)
                    if tm:
                        bt = tm.group(1)
                        if bt not in seen:
                            base_types.append(bt)
                            seen.add(bt)

                # Parse the predicate pattern: "carry(ball, gripper)"
                pm = re.match(r'([\w-]+)\(([^)]*)\)', pred_str.strip())
                if not pm:
                    continue
                pred_name = pm.group(1)
                raw_args = [a.strip() for a in pm.group(2).split(",")]
                pred_args = []
                for arg in raw_args:
                    if arg in seen:  # it's a type name → variable
                        pred_args.append(("var", arg))
                    else:
                        pred_args.append(("const", arg))

                num_feats[feat_name] = NumericFeature(
                    name=feat_name,
                    base_types=base_types,
                    pred_name=pred_name,
                    pred_args=pred_args,
                )

            elif feat_name.startswith("B"):
                # Boolean feature: "at-robby(rooma)" or "at-robby(rooma, ...)"
                pm = re.match(r'([\w-]+)\(([^)]*)\)', rest.strip())
                if pm:
                    pred_name = pm.group(1)
                    args = [a.strip() for a in pm.group(2).split(",")]
                    atom = (pred_name,) + tuple(args)
                else:
                    atom = (rest.strip(),)
                bool_feats[feat_name] = BooleanFeature(name=feat_name, atom=atom)

    return subtypes, num_feats, bool_feats


# ---------------------------------------------------------------------------
# Policy-file parsing
# ---------------------------------------------------------------------------

@dataclass
class Condition:
    feature: str   # "N0", "B0", …
    op: str        # ">0", "=0", "=T", "=F"


@dataclass
class PolicyRule:
    number: int
    conditions: list[Condition]
    # action_template is None for the terminal (goal) rule
    action_name: Optional[str]
    # Each element of action_args is either:
    #   ("features", [int, ...])   — feature indices
    #   ("const",    str)          — constant object name
    action_args: Optional[list]


def _parse_policy(path: str) -> list[PolicyRule]:
    """
    Parse a .policy file and return an ordered list of PolicyRule.

    Each non-empty line has the form:
        { B0=T B1=F N0>0 N1=0 } : action(args)

    Rules are returned in file order; first matching rule wins during execution.
    An empty trailing line (if any) is treated as the terminal/goal indicator.
    """
    rules: list[PolicyRule] = []

    with open(path) as f:
        for num, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                # Empty line = terminal rule
                rules.append(PolicyRule(
                    number=num, conditions=[], action_name=None, action_args=None))
                continue

            # "{ ... } : action"
            m = re.match(r'\{([^}]*)\}\s*:\s*(.+)', line)
            if not m:
                continue
            cond_str, action_str = m.group(1).strip(), m.group(2).strip()

            # Parse conditions
            conditions = []
            for token in cond_str.split():
                # tokens: "B0=T", "B1=F", "N0>0", "N1=0"
                cm = re.match(r'([BN]\d+)(=T|=F|>0|=0)', token)
                if cm:
                    conditions.append(Condition(feature=cm.group(1), op=cm.group(2)))

            # Parse action template, e.g.:
            #   move(rooma,roomb)
            #   pick((N0,N3),rooma)
            #   drop((N2),roomb)
            am = re.match(r'([\w-]+)\((.+)\)$', action_str)
            if not am:
                continue
            action_name = am.group(1)
            args_raw = am.group(2)

            # Tokenise args, respecting inner parentheses
            action_args = _parse_action_args(args_raw)

            rules.append(PolicyRule(
                number=num,
                conditions=conditions,
                action_name=action_name,
                action_args=action_args,
            ))

    return rules


def _parse_action_args(args_raw: str) -> list:
    """
    Parse the argument list of an action template.

    Handles both plain constants and feature tuples like (N0,N3).
    Returns a list where each element is either:
        ("features", [0, 3])   — from "(N0,N3)"
        ("const", "rooma")     — from "rooma"
    """
    result = []
    i = 0
    s = args_raw.strip()
    while i < len(s):
        if s[i] == "(":
            # Find matching ")"
            j = s.index(")", i)
            inner = s[i+1:j]
            # Parse "N0,N3" → [0, 3]
            indices = [int(re.match(r'N(\d+)', t.strip()).group(1))
                       for t in inner.split(",")]
            result.append(("features", indices))
            i = j + 1
            # Skip comma
            if i < len(s) and s[i] == ",":
                i += 1
        else:
            # Read until next comma (at the top level)
            j = s.find(",", i)
            if j == -1:
                j = len(s)
            token = s[i:j].strip()
            if token:
                result.append(("const", token))
            i = j + 1
    return result


# ---------------------------------------------------------------------------
# Feature evaluation
# ---------------------------------------------------------------------------

def _subtype_objects(subtypes: dict, base_type: str) -> list[str]:
    """Return the list of objects belonging to the subtype of base_type."""
    for label, info in subtypes.items():
        if info["base_type"] == base_type:
            return info["objects"]
    return []


def _eval_numeric(feat: NumericFeature, state: frozenset, subtypes: dict) -> int:
    """Count ground atoms that match the feature's predicate pattern."""
    # Build candidate lists for each variable position
    var_candidates: dict[str, list[str]] = {}
    for bt in feat.base_types:
        var_candidates[bt] = _subtype_objects(subtypes, bt)

    # Variable positions in pred_args tell us which arg positions loop over objects
    # Build the variable order from pred_args
    var_order = [arg[1] for arg in feat.pred_args if arg[0] == "var"]

    # Enumerate all combinations
    candidate_lists = [var_candidates.get(bt, []) for bt in var_order]
    count = 0
    for combo in product(*candidate_lists):
        binding = dict(zip(var_order, combo))
        args = []
        for kind, val in feat.pred_args:
            args.append(binding[val] if kind == "var" else val)
        atom = (feat.pred_name,) + tuple(args)
        if atom in state:
            count += 1
    return count


def _eval_boolean(feat: BooleanFeature, state: frozenset) -> bool:
    return feat.atom in state


def _evaluate_features(
    state: frozenset,
    num_feats: dict[str, NumericFeature],
    bool_feats: dict[str, BooleanFeature],
    subtypes: dict,
) -> dict[str, object]:
    """Return a dict mapping feature name → value (int or bool)."""
    vals: dict[str, object] = {}
    for name, feat in num_feats.items():
        vals[name] = _eval_numeric(feat, state, subtypes)
    for name, feat in bool_feats.items():
        vals[name] = _eval_boolean(feat, state)
    return vals


# ---------------------------------------------------------------------------
# Rule matching
# ---------------------------------------------------------------------------

def _rule_satisfied(rule: PolicyRule, feat_vals: dict) -> bool:
    for cond in rule.conditions:
        val = feat_vals.get(cond.feature)
        if val is None:
            return False
        if cond.op == ">0" and not (isinstance(val, int) and val > 0):
            return False
        if cond.op == "=0" and not (isinstance(val, int) and val == 0):
            return False
        if cond.op == "=T" and val is not True:
            return False
        if cond.op == "=F" and val is not False:
            return False
    return True


# ---------------------------------------------------------------------------
# Action grounding
# ---------------------------------------------------------------------------

def _find_witness(feat: NumericFeature, state: frozenset, subtypes: dict) -> Optional[dict[str, str]]:
    """
    Find one witness binding {type_name -> object_name} for the feature.

    Returns None if no witness exists.
    """
    var_order = [arg[1] for arg in feat.pred_args if arg[0] == "var"]
    candidate_lists = [_subtype_objects(subtypes, bt) for bt in var_order]

    for combo in product(*candidate_lists):
        binding = dict(zip(var_order, combo))
        args = []
        for kind, val in feat.pred_args:
            args.append(binding[val] if kind == "var" else val)
        if (feat.pred_name,) + tuple(args) in state:
            return binding
    return None


def _ground_action(
    rule: PolicyRule,
    state: frozenset,
    num_feats: dict[str, NumericFeature],
    subtypes: dict,
    domain_actions: dict,
) -> Optional[tuple[str, list[str]]]:
    """
    Ground the action template of a rule to a specific (action_name, [args]) call.

    Feature witnesses are matched to PDDL action parameters by type.
    Returns (action_name, [arg_name, ...]) or None on failure.
    """
    if rule.action_name is None:
        return None

    action_schema = domain_actions[rule.action_name]
    params = action_schema.parameters
    action_args = rule.action_args or []

    # Collect object bindings from feature witnesses: type_name → object_name
    type_to_obj: dict[str, str] = {}

    for arg_spec in action_args:
        if arg_spec[0] != "features":
            continue
        feat_indices = arg_spec[1]
        for idx in feat_indices:
            feat_name = f"N{idx}"
            feat = num_feats[feat_name]
            witness = _find_witness(feat, state, subtypes)
            if witness is None:
                print(f"  [warn] No witness found for feature {feat_name}", file=sys.stderr)
                return None
            type_to_obj.update(witness)

    # Collect constants from the action args
    # Constants in the template appear as ("const", name) entries
    # We match them to action params by type using the objects dict (available via domain)
    # For now, store constants by their name; we'll match by type below.
    const_names = [arg[1] for arg in action_args if arg[0] == "const"]

    # Build object-name → type-name map from params where the object is already known
    # We need the problem objects for type lookup — use the domain action's signature
    # to determine what types each parameter expects, then assign:
    # 1. Parameters whose type is covered by a feature witness → use type_to_obj
    # 2. Remaining parameters → filled from const_names in order

    result_args: list[Optional[str]] = [None] * len(params)

    # Pass 1: assign feature witnesses by type
    for i, param in enumerate(params):
        type_name = _type_name(param.type_tags)
        if type_name in type_to_obj:
            result_args[i] = type_to_obj[type_name]

    # Pass 2: fill remaining slots from constants in order
    const_iter = iter(const_names)
    for i, arg in enumerate(result_args):
        if arg is None:
            try:
                result_args[i] = next(const_iter)
            except StopIteration:
                print(f"  [error] Not enough constants for action {rule.action_name}", file=sys.stderr)
                return None

    if any(a is None for a in result_args):
        print(f"  [error] Could not fully ground action {rule.action_name}", file=sys.stderr)
        return None

    grounded_args = [arg for arg in result_args if arg is not None]
    return rule.action_name, grounded_args


# ---------------------------------------------------------------------------
# Action application
# ---------------------------------------------------------------------------

def _apply_action(
    state: frozenset,
    action_name: str,
    args: list[str],
    domain_actions: dict,
) -> frozenset:
    """Apply a grounded action to a state, returning the successor state."""
    schema = domain_actions[action_name]
    binding = {param.name: args[i] for i, param in enumerate(schema.parameters)}

    new_state = set(state)
    for clause in _iter_conjuncts(schema.effect):
        if isinstance(clause, Predicate):
            new_state.add(_ground_formula_atom(clause, binding))
        elif isinstance(clause, Not) and isinstance(clause.argument, Predicate):
            new_state.discard(_ground_formula_atom(clause.argument, binding))
    return frozenset(new_state)


def _check_preconditions(
    state: frozenset,
    action_name: str,
    args: list[str],
    domain_actions: dict,
) -> bool:
    schema = domain_actions[action_name]
    binding = {param.name: args[i] for i, param in enumerate(schema.parameters)}

    for clause in _iter_conjuncts(schema.precondition):
        if isinstance(clause, Predicate):
            if _ground_formula_atom(clause, binding) not in state:
                return False
        elif isinstance(clause, Not) and isinstance(clause.argument, Predicate):
            if _ground_formula_atom(clause.argument, binding) in state:
                return False
        else:
            return False
    return True


# ---------------------------------------------------------------------------
# Goal check
# ---------------------------------------------------------------------------

def _is_goal(state: frozenset, goal_atoms: frozenset) -> bool:
    return goal_atoms.issubset(state)


# ---------------------------------------------------------------------------
# Main execution loop
# ---------------------------------------------------------------------------

def execute_policy(domain_file: str, problem_file: str, abs_file: str, policy_file: str,
                   verbose: bool = True):
    # --- Parse PDDL ---
    domain = parse_domain(domain_file)
    problem = parse_problem(problem_file)

    state = _make_initial_state(problem)
    goal_atoms = _goal_atoms(problem)
    domain_actions = {action.name: action for action in domain.actions}

    # --- Parse .abs file ---
    subtypes, num_feats, bool_feats = _parse_abs(abs_file)

    # --- Parse .policy file ---
    rules = _parse_policy(policy_file)
    # Sort by rule number; the policy is applied in order — first matching rule wins.
    rules.sort(key=lambda r: r.number)

    print(f"Loaded {len(rules)} policy rules, "
          f"{len(num_feats)} numeric features, "
          f"{len(bool_feats)} boolean features.")
    print(f"Initial state has {len(state)} atoms.")
    print()

    step = 0
    max_steps = 10_000  # safety limit

    while step < max_steps:
        step += 1

        # Check goal
        if _is_goal(state, goal_atoms):
            print(f"Goal reached after {step - 1} steps.")
            return True

        # Evaluate features
        feat_vals = _evaluate_features(state, num_feats, bool_feats, subtypes)

        if verbose and (step <= 3 or step % 10 == 0):
            _print_state_summary(step, state, feat_vals)

        # Find first applicable rule
        applicable = None
        for rule in rules:
            if rule.action_name is None:
                # Terminal rule — goal should have been caught above
                print("Reached terminal rule without satisfying goal — check policy/goal.")
                return False
            if _rule_satisfied(rule, feat_vals):
                applicable = rule
                break

        if applicable is None:
            print(f"Step {step}: No applicable rule found. Dead end.")
            _print_state_summary(step, state, feat_vals)
            return False

        # Ground and apply action
        grounded = _ground_action(applicable, state, num_feats, subtypes, domain_actions)
        if grounded is None:
            print(f"Step {step}: Failed to ground action for rule {applicable.number}.")
            return False

        action_name, args = grounded
        action_str = f"{action_name}({', '.join(args)})"

        if not _check_preconditions(state, action_name, args, domain_actions):
            print(f"Step {step}: Precondition failed for {action_str}.")
            return False

        print(f"Step {step:3d}: rule {applicable.number:2d} → {action_str}")

        state = _apply_action(state, action_name, args, domain_actions)

    print(f"Exceeded maximum step limit ({max_steps}).")
    return False


def _print_state_summary(step: int, state: frozenset, feat_vals: dict):
    vals_str = "  ".join(
        f"{k}={'T' if v is True else 'F' if v is False else v}"
        for k, v in sorted(feat_vals.items())
    )
    print(f"  [features] {vals_str}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(1)

    domain_file, problem_file, abs_file, policy_file = sys.argv[1:]
    success = execute_policy(domain_file, problem_file, abs_file, policy_file)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
