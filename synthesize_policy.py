#!/usr/bin/env python3
"""
Policy Synthesis Pipeline Wrapper

Automates all steps of the ABS policy synthesis pipeline:
1. Generate mutex groups
2. Run ABS abstraction
3. Run BQS solver

Usage:
    python3 synthesize_policy.py <domain.pddl> <problem.pddl> [--output-dir <dir>]

Example:
    python3 synthesize_policy.py \\
        domains/Gripper-Sim/domain.pddl \\
        domains/Gripper-Sim/prob1-1.pddl \\
        --output-dir ./generation/Gripper-Sim
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run_command(cmd, description, cwd=None):
    """Execute a shell command and handle errors."""
    print(f"\n[*] {description}")
    print(f"    Command: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=False,
            text=True,
            check=True
        )
        print(f"[✓] {description} completed successfully")
        return result
    except subprocess.CalledProcessError as e:
        print(f"[✗] Error during {description}")
        print(f"    Exit code: {e.returncode}")
        sys.exit(1)
    except FileNotFoundError as e:
        print(f"[✗] Command not found: {e}")
        sys.exit(1)


def get_problem_name(problem_path):
    """Extract problem name from problem file path.
    
    E.g., domains/Gripper-Sim/prob1-1.pddl -> prob1-1
    """
    return Path(problem_path).stem


def get_domain_name(domain_path):
    """Extract domain directory name from domain file path.
    
    E.g., domains/Gripper-Sim/domain.pddl -> Gripper-Sim
    """
    return Path(domain_path).parent.name


def setup_working_directory(work_dir, domain_path, problem_path):
    """Set up working directory with domain and problem files."""
    work_dir = Path(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)
    
    # Copy domain and problem files
    domain_dest = work_dir / "domain.pddl"
    problem_dest = work_dir / Path(problem_path).name
    
    shutil.copy2(domain_path, domain_dest)
    shutil.copy2(problem_path, problem_dest)
    
    print(f"[✓] Working directory set up: {work_dir}")
    return domain_dest, problem_dest


def get_repo_root():
    """Get the repository root directory."""
    script_dir = Path(__file__).resolve().parent
    return script_dir


def synthesize_policy(domain_path, problem_path, output_dir=None, keep_intermediates=False):
    """Run the complete policy synthesis pipeline.
    
    Args:
        domain_path: Path to domain.pddl file
        problem_path: Path to problem.pddl file
        output_dir: Directory to store final outputs. If None, uses generation/<domain-name>/
        keep_intermediates: If True, keep intermediate files; otherwise use temp directory
    
    Returns:
        Tuple of (policy_file, abs_file) paths
    """
    domain_path = Path(domain_path).resolve()
    problem_path = Path(problem_path).resolve()
    repo_root = get_repo_root()
    
    # Validate input files
    if not domain_path.exists():
        print(f"[✗] Domain file not found: {domain_path}")
        sys.exit(1)
    if not problem_path.exists():
        print(f"[✗] Problem file not found: {problem_path}")
        sys.exit(1)
    
    problem_name = get_problem_name(problem_path)
    domain_name = get_domain_name(domain_path)
    
    # Set up output directory
    if output_dir is None:
        output_dir = repo_root / "generation" / domain_name
    else:
        output_dir = Path(output_dir)
    
    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"[*] Output directory: {output_dir}")
    
    # Create working directory for intermediate files
    if keep_intermediates:
        work_dir = output_dir / "working"
        cleanup_work = False
    else:
        work_dir = tempfile.mkdtemp(prefix=f"abs_synthesis_{problem_name}_")
        cleanup_work = True
    
    try:
        print(f"[*] Working directory: {work_dir}")
        
        # Step 1: Set up working directory with domain and problem files
        domain_work, problem_work = setup_working_directory(work_dir, domain_path, problem_path)
        
        # Step 2: Generate mutex groups
        addition_file = Path(work_dir) / "addition"
        genMutex_script = repo_root / "translate" / "genMutexAddition.py"
        
        if not genMutex_script.exists():
            print(f"[✗] genMutexAddition.py not found at {genMutex_script}")
            sys.exit(1)
        
        run_command(
            ["python3", str(genMutex_script), str(domain_work), str(problem_work), str(addition_file)],
            "Generating mutex groups"
        )
        
        # Step 3: Run ABS abstraction
        abs_executable = repo_root / "bin" / "ABS"
        if not abs_executable.exists():
            print(f"[✗] ABS executable not found at {abs_executable}")
            print("[!] Please build the project first:")
            print(f"    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release")
            print(f"    cmake --build build -j$(nproc)")
            sys.exit(1)
        
        run_command(
            [str(abs_executable), str(work_dir), problem_name, str(output_dir)],
            "Running ABS abstraction",
            cwd=str(repo_root)
        )
        
        # Verify ABS outputs
        qnp_file = output_dir / f"{problem_name}.qnp"
        abs_file = output_dir / f"{problem_name}.abs"
        
        if not qnp_file.exists():
            print(f"[✗] ABS did not generate {qnp_file}")
            sys.exit(1)
        if not abs_file.exists():
            print(f"[✗] ABS did not generate {abs_file}")
            sys.exit(1)
        
        # Step 4: Run BQS solver
        bqs_executable = repo_root / "ext" / "BQS" / "BQS"
        if not bqs_executable.exists():
            print(f"[✗] BQS executable not found at {bqs_executable}")
            print("[!] Please check that the BQS solver is available in ext/BQS/")
            sys.exit(1)
        
        policy_file = output_dir / f"{problem_name}.policy"
        run_command(
            [str(bqs_executable), str(qnp_file), str(policy_file)],
            "Running BQS solver"
        )
        
        # Verify final output
        if not policy_file.exists():
            print(f"[✗] BQS did not generate {policy_file}")
            sys.exit(1)
        
        print("\n" + "="*70)
        print("SYNTHESIS COMPLETE")
        print("="*70)
        print(f"[✓] Policy file:  {policy_file}")
        print(f"[✓] Feature file: {abs_file}")
        print(f"\nYou can now execute the policy with:")
        print(f"    python3 execute_policy.py \\")
        print(f"        {domain_path} \\")
        print(f"        {problem_path} \\")
        print(f"        {abs_file} \\")
        print(f"        {policy_file}")
        print("="*70)
        
        return policy_file, abs_file
        
    finally:
        # Clean up working directory if temporary
        if cleanup_work and Path(work_dir).exists():
            shutil.rmtree(work_dir)
            print(f"\n[*] Cleaned up temporary working directory: {work_dir}")


def main():
    parser = argparse.ArgumentParser(
        description="ABS Policy Synthesis Pipeline",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 synthesize_policy.py domains/Gripper-Sim/domain.pddl domains/Gripper-Sim/prob1-1.pddl
  python3 synthesize_policy.py domains/Ferry/domain.pddl domains/Ferry/prob01.pddl --output-dir ./my_outputs
  python3 synthesize_policy.py --keep-intermediates domains/Transport/domain.pddl domains/Transport/p01.pddl
        """
    )
    
    parser.add_argument(
        "domain",
        metavar="DOMAIN",
        help="Path to domain.pddl file"
    )
    parser.add_argument(
        "problem",
        metavar="PROBLEM",
        help="Path to problem.pddl file"
    )
    parser.add_argument(
        "--output-dir",
        "-o",
        metavar="DIR",
        default=None,
        help="Output directory for final files (default: generation/<domain-name>/)"
    )
    parser.add_argument(
        "--keep-intermediates",
        "-k",
        action="store_true",
        help="Keep intermediate files in working/ subdirectory of output-dir"
    )
    
    args = parser.parse_args()
    
    try:
        synthesize_policy(
            args.domain,
            args.problem,
            output_dir=args.output_dir,
            keep_intermediates=args.keep_intermediates
        )
    except KeyboardInterrupt:
        print("\n[!] Synthesis interrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\n[✗] Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
