# Guide: Implementing Analysis Extensions

Analysis extensions provide a powerful mechanism to attach extra, problem-specific computations and bookkeeping to the scheduling state space exploration. They act as orthogonal observers that can compute metrics, collect statistics, or validate properties without influencing the core scheduling logic.

This guide is for programmers who want to implement their own analysis extensions.

## When to Use Analysis Extensions

Analysis extensions are the right tool when you need to **observe and analyze** the state space exploration. They are ideal for:

-   **Computing metrics:** Calculating derived timing properties like reaction times or data age for specific event chains.
-   **Collecting statistics:** Aggregating data across the entire exploration, such as the maximum throughput, response/activation jitter or resource utilization.
-   **Tracing properties:** Following the evolution of a custom value through the scheduling states.
-   **Validating invariants:** Checking if certain properties hold true in every reachable scheduling state.

The key principle is that the extension is a passive observer. It gathers data but does not change the outcome of the scheduling analysis.

## When NOT to Use Analysis Extensions

This mechanism is **not** the right place if you need to modify the behavior of the scheduler. Do **not** use an analysis extension if you want to:

-   Modify scheduling choices.
-   Change which jobs may be selected to run next.
-   Alter which core a job is dispatched on.
-	Change how a system state is encoded and/or evolves.

If you need to influence the state-space exploration, you should instead alter the core exploration logic or the job priority rules.

## How to Implement an Analysis Extension

Implementing an extension involves creating a few cooperating classes that hook into the state-space exploration lifecycle.

### Prerequisites: Enabling Extensions

First, you must enable extensions at compile time. This is done by adding `-DCONFIG_ANALYSIS_EXTENSIONS` to your compiler flags or enabling the corresponding option in your CMake configuration. Without this flag, the core data structures will not contain the necessary extension hooks, and no extension will not be activated.

### The Building Blocks

An analysis extension is composed of up to four C++ classes:

1.  **Problem Extension (Optional)**
    -   Derives from `Problem_extension`.
    -   Holds static configuration parsed from an input file (e.g., a YAML or csv file). This data is read once and is immutable during the analysis. It can contain a list of entities to monitor, configuration flags, or other inputs required by your analysis.

2.  **State Space Data Extension**
    -   Derives from `State_space_data_extension<Time>`.
    -   There is a single instance of this class per state space exploration. It is shared by all states.
    -   Use it to store global data: aggregated results (max/min values), lookup tables, or pre-computed values that are needed by per-state calculations.

3.  **State Extension**
    -   Derives from `State_extension<Time>`.
    -   This is where the main logic resides. An instance of this class is attached to every scheduling state (`Schedule_state<Time>`).
    -   It holds per-state data and implements the hook methods to update this data as new states are created and merged.

4.  **Activation Wrapper**
    -   A simple templated class that inherits from `Extension<Time, StateExt, StateSpaceDataExt>`.
    -   It wires the state and space-data extensions together and provides a static `activate()` method to register them with the analysis framework.

### The Lifecycle and Key Methods (Hooks)

The framework invokes virtual methods on your `State_extension` subclass at key points during the exploration. You only need to override the ones you need.

-   `construct(...)`: Called when a new state is created.
    -  Two variants are for the initial "root" state of the exploration. This is where you set up the initial values for your per-state data.
    -   Another variant is for successor states. It receives the parent state, the job that was just dispatched, and its start/finish time intervals. This is where you implement your incremental update logic, deriving the child state's extension data from the parent state's.

-   `reset(...)`: The framework uses object pooling to reduce memory allocation overhead. If a state object is reused from a pool, `reset()` is called instead of `construct()`. Your `reset()` implementation should mirror the corresponding `construct()` signature and efficiently reset the object's state, for example by overwriting its contents in-place.

-   `merge(..., survivor, other)`: Called when the exploration engine determines that two states can be merged (e.g., one is dominated by the other or both have a similar future). You must implement a **conservative join** of the data from your extension in the `other` state into the `survivor` state. The resulting merged data must over-approximate all possibilities represented by both original states to keep the analysis sound.

### Step-by-Step Guide

1.  **(Optional) Define a Problem Extension:** If your analysis requires configuration, create a class that inherits from `Problem_extension`.
2.  **Define the Space Data Extension:** Create a class that inherits from `State_space_data_extension` to hold global/aggregated data/results.
3.  **Define the State Extension:** Create a class that inherits from `State_extension<Time>`. Implement the core logic here by overriding the `construct`, `reset`, and `merge` hooks.
4.  **Define the Activation Wrapper:** Create a simple wrapper class that inherits from `Extension<Time, YourStateExtension, YourSpaceDataExtension>`. There is normally no need to add any method or data to that class.
5.  **Activate the Extension:** In your application's setup code (e.g., in the constructor of `State_space<Time>`), call `YourAnalysisExtension<Time>::activate(...)` **once**. Any arguments passed to `activate(...)` will be forwarded to the constructor of your state space data extension. **IMPORTANT:** if you call `YourAnalysisExtension<Time>::activate(...)` more than once, then the extension `YourAnalysisExtension<Time>` will also be instantiated and executed more than once.

### Minimal Implementation Template

Here is a skeletal example. You can use this as a starting point for your own extension.

```cpp
#include "global/extension.h"
#include "global/schedule_state.h"
#include "global/state_space_data.h"
#include "problem/problem_extension.h"

// 1. (Optional) Problem-level data holder
struct MyProblemExtension : public Problem_extension {
	// configuration data parsed once
	explicit MyProblemExtension(/* config args */) { /* store config */ }
};

// 2. Space (global) data
template<class Time>
class MySpaceDataExtension : public State_space_data_extension<Time> {
public:
	// global lookup tables, aggregates, counters...
	explicit MySpaceDataExtension(/* forwarded args from activate */) {
		// build lookup tables, precompute constants, reserve buffers for results...
	}

	// helper API for state extensions to submit results
	void submit_value(unsigned idx, Time v) { /* aggregate conservatively */ }
	Time result(unsigned idx) const { /* return aggregate */ return Time(); }
};

// 3. Per-state data & incremental update logic
template<class Time>
class MyStateExtension : public State_extension<Time> {
	// per-state buffers / snapshots
	std::vector<Time> local_values; 
public:
	MyStateExtension() = default;

	// Hook for root state creation
	void construct(size_t ext_id, size_t space_ext_id, const Schedule_state<Time>& st,
				   unsigned int num_procs, const State_space_data<Time>& ssd) override {
		auto* space = ssd.get_extensions().get<MySpaceDataExtension<Time>>(space_ext_id);
		// initialize per-state data using global indices
		local_values.assign(/*size*/, Time());
	}

	// Hook for successor state creation (incremental update)
	void construct(size_t ext_id, size_t space_ext_id, const Schedule_state<Time>& child,
				   const Schedule_state<Time>& parent, Job_index j,
				   const Interval<Time>& start_t, const Interval<Time>& finish_t,
				   const Job_set& scheduled_jobs,
				   const std::vector<Job_index>& jobs_with_pending_succ,
				   const std::vector<const Job<Time>*>& ready_succ_jobs,
				   const State_space_data<Time>& ssd,
				   Time next_source_job_rel, unsigned int ncores = 1) override {
		// incremental update from parent.local_values
		auto* space = ssd.get_extensions().get<MySpaceDataExtension<Time>>(space_ext_id);
		// update changed portions only
	}

	// Hook for state merging
	void merge(size_t ext_id, const Schedule_state<Time>& survivor,
			   const Schedule_state<Time>& other) override {
		// conservative join of local_values with other's values
	}
    
    // IMPORTANT: Remember to also implement the mirror 'reset' methods for each 'construct' method you implement
};

// 4. Activation wrapper
template<class Time>
class MyAnalysisExtension : public Extension<Time, MyStateExtension<Time>, MySpaceDataExtension<Time>> {};

// 5. Activation (in the constructor of State_space, before starting to explore)
void register_my_extension() {
	MyAnalysisExtension<MyTimeType>::activate(state_space_data, /* args to MySpaceDataExtension ctor */);
}
```

### Accessing Your Extension's Data

You can access your extension instances from other parts of the code (e.g., for retrieving final results or debugging).

From state space data (to get global results):
```cpp
State_space_data<Time>& ssd = ...; 
auto* space_ext = ssd.get_extensions().get<YourSpaceDataExt>();
```

From a state (to get per-state data):
```cpp
Schedule_state<Time>& st = ...; 
auto* my_ext = st.get_extensions().get<YourStateExt<Time>>();
```

The method `get<YourStateExt<Time>>()` retrieves extensions with dynamic casting. There is a second, more efficient, and thus preferred, method that takes the ID of the extension as parameter. That ID can be recovered as the return value from the extensions registering methods.

## Best Practices and Design Guidelines

-   **Keep it Observational:** Your extension must **not** influence scheduling decisions. Treat all hooks as read-only with respect to the core scheduler state.
-   **Be Efficient:** The `construct` and `reset` hooks for successor states is on the critical path. Make it fast. Copy data from the parent state and incrementally update only what has changed. Avoid expensive re-computations from scratch.
-   **Merge Conservatively:** Your `merge` logic is crucial for correctness. The merged state must represent a superset of the possibilities of the original states. For example, when merging intervals, take the union. When merging a "max value seen so far", take the maximum of both. An incorrect merge can lead to unsound results.
-   **Manage Memory Wisely** Tens of thousands of states may co-exist in memory. Keep the memory footprint of your state extension as small as possible.
-   **Thread Safety** If the exploration is single-threaded (typical), no extra synchronization is needed. If you later parallelize state expansion, confine all mutation during a child state construction to that child’s extension object. Space-level aggregates should either use atomic operations or be updated in a controlled (serialized) post-processing step.
-  **Enabling / Disabling** The extension can be enabled for each analysis independently without need to recompile (for example, based on command line arguments or custom configuration files). *Don't enable* your extension whenever it is not required to reduce the analysis runtime and memory overhead, i.e., do not call `MyAnalysisExtension<MyTimeType>::activate(...)` unless you need to extecute that extension. 