# Project Guidelines

This repository uses a single root `AGENTS.md` as the shared instruction file for coding agents. Keep instructions here general enough to work for both Claude and GitHub Copilot, and do not add a second workspace-wide instruction file unless this one is being replaced.

## Project Overview

This code base synthesizes general policies for planning problems.

- Domains and problem instances live under `domains/`.
- The main abstraction implementation is the C++ code in `ABS/`.
- Mutex-group generation is handled by the Python scripts in `translate/`.
- The external BQNP solver is in `ext/BQS/`.
- Generated artifacts are typically written under `generation/<domain>/`.

Read `README.md` before changing the pipeline or command-line usage.

## Working Rules

- Prefer minimal, localized changes that preserve the current command-line interface and file formats.
- Do not rename pipeline artifacts such as `.abs`, `.info`, `.qnp`, or `.policy` unless the task explicitly requires it.
- Treat `ext/`, `VAL/`, and `eigen-3.4.0/` as external code. Avoid editing them unless the task is specifically about those dependencies.
- When changing the synthesis pipeline, verify the full path from PDDL input to generated policy, not just the intermediate files.

## Build And Verify

Build with CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The main executable is `bin/ABS`.

If a change affects the main workflow, validate the pipeline with a real domain/problem pair when feasible.

## End-To-End Pipeline

The normal workflow has three stages.

1. Generate mutex groups:

   ```sh
   python3 translate/genMutexAddition.py domains/Gripper-Sim/domain.pddl domains/Gripper-Sim/prob1-1.pddl domains/Gripper-Sim/addition
   ```

   Important: the `addition` file must be written into the same domain directory as `domain.pddl` and the problem file. Later steps expect that layout.

2. Run ABS to produce the abstraction and QNP instance:

   ```sh
   ./bin/ABS domains/Gripper-Sim prob1-1 generation/Gripper-Sim
   ```

3. Run BQS on the generated QNP file to produce the final policy:

   ```sh
   ./ext/BQS/BQS generation/Gripper-Sim/prob1-1.qnp ./generation/Gripper-Sim/prob1-1.policy
   ```

The expected end result is a `.policy` file containing the BQNP policy.

## Repository Conventions

- Keep Linux command examples current. This repository is actively used on Linux.
- Prefer commands and paths that work from the repository root.
- When documenting or automating the workflow, use domain/problem examples that already exist in `domains/`.
- If you update the pipeline, keep `README.md` and this file consistent.