cmake_minimum_required(VERSION 3.14)
project(ABS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ── VAL static library ────────────────────────────────────────────────────────
set(VAL_SOURCES
    VAL/AccumulatedDelta.cpp
    VAL/Action.cpp
    VAL/CausalGraph.cpp
    VAL/DebugWriteController.cpp
    VAL/Environment.cpp
    VAL/Evaluator.cpp
    VAL/Events.cpp
    VAL/FastEnvironment.cpp
    VAL/FuncAnalysis.cpp
    VAL/FuncExp.cpp
    VAL/graphconstruct.cpp
    VAL/HowAnalyser.cpp
    VAL/instantiation.cpp
    VAL/InstPropLinker.cpp
    VAL/LaTeXSupport.cpp
    VAL/LibLink.cpp
    VAL/Ownership.cpp
    VAL/pddl+.cpp
    VAL/PinguPlanGenerator.cpp
    VAL/Plan.cpp
    VAL/Polynomial.cpp
    VAL/PrettyPrinter.cpp
    VAL/Proposition.cpp
    VAL/ptree.cpp
    VAL/random.cpp
    VAL/RelaxTranslator.cpp
    VAL/RepairAdvice.cpp
    VAL/RobustAnalyse.cpp
    VAL/SASActions.cpp
    VAL/SimpleEval.cpp
    VAL/State.cpp
    VAL/TIM.cpp
    VAL/TimSupport.cpp
    VAL/ToFunction.cpp
    VAL/TrajectoryConstraints.cpp
    VAL/typecheck.cpp
    VAL/TypedAnalyser.cpp
    VAL/Utils.cpp
    VAL/Validator.cpp
    VAL/valLib.cpp
)

add_library(VAL_lib STATIC ${VAL_SOURCES})
target_include_directories(VAL_lib PUBLIC ${CMAKE_SOURCE_DIR}/VAL)
target_compile_options(VAL_lib PRIVATE
    -Wno-deprecated
    -Wno-deprecated-declarations
    -Wno-unused-result
    -Wno-write-strings
)

# ── ABS executable ────────────────────────────────────────────────────────────
set(ABS_SOURCES
    ABS/abstraction.cpp
    ABS/action.cpp
    ABS/axiom.cpp
    ABS/derived_predicate.cpp
    ABS/formula.cpp
    ABS/mainABS.cpp
    ABS/normal_form.cpp
    ABS/np.cpp
    ABS/object.cpp
    ABS/pddl.cpp
    ABS/predicate.cpp
    ABS/proposition.cpp
    ABS/state.cpp
    ABS/utils/parameter_generator.cpp
)

add_executable(ABS ${ABS_SOURCES})
target_include_directories(ABS PRIVATE
    ${CMAKE_SOURCE_DIR}/ABS
    ${CMAKE_SOURCE_DIR}/VAL
    ${CMAKE_SOURCE_DIR}/eigen-3.4.0
)
target_link_libraries(ABS PRIVATE VAL_lib)
target_compile_options(ABS PRIVATE -Wno-deprecated -Wno-deprecated-declarations)

# Stack size: the VS project sets 50 MB; replicate with a linker flag on Linux.
target_link_options(ABS PRIVATE -Wl,-z,stacksize=52428800)
