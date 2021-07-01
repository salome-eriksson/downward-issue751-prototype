#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include "algorithms/ordered_set.h"
#include "evaluator.h"
#include "evaluation_result.h"
#include "operator_cost.h"
#include "operator_id.h"
#include "plan_manager.h"
#include "search_progress.h"
#include "search_space.h"
#include "search_statistics.h"
#include "state_registry.h"
#include "task_proxy.h"

#include <vector>

namespace options {
class OptionParser;
class Options;
}

namespace ordered_set {
template<typename T>
class OrderedSet;
}

namespace successor_generator {
class SuccessorGenerator;
}

namespace utils {
enum class Verbosity;
}

enum SearchStatus {IN_PROGRESS, TIMEOUT, FAILED, SOLVED};

class SearchEngine {
    SearchStatus status;
    bool solution_found;
    Plan plan;
protected:
    // Hold a reference to the task implementation and pass it to objects that need it.
    const std::shared_ptr<AbstractTask> task;
    // Use task_proxy to access task information.
    TaskProxy task_proxy;

    PlanManager plan_manager;
    StateRegistry state_registry;
    const successor_generator::SuccessorGenerator &successor_generator;
    SearchSpace search_space;
    SearchProgress search_progress;
    SearchStatistics statistics;
    std::shared_ptr<Evaluator> real_g_evaluator;
    int bound;
    bool is_unit_cost;
    double max_time;
    const utils::Verbosity verbosity;

    virtual void initialize() {}
    virtual SearchStatus step() = 0;

    void set_plan(const Plan &plan);
    bool check_goal_and_set_plan(const State &state);
public:
    SearchEngine(const options::Options &opts);
    virtual ~SearchEngine();
    virtual void print_statistics() const = 0;
    virtual void save_plan_if_necessary();
    bool found_solution() const;
    SearchStatus get_status() const;
    const Plan &get_plan() const;
    void search();
    const SearchStatistics &get_statistics() const {return statistics;}
    void set_bound(int b) {bound = b;}
    int get_bound() {return bound;}
    PlanManager &get_plan_manager() {return plan_manager;}

    /* The following three methods should become functions as they
       do not require access to private/protected class members. */
    static void add_pruning_option(options::OptionParser &parser);
    static void add_options_to_parser(options::OptionParser &parser);
    static void add_succ_order_options(options::OptionParser &parser);
};

/*
  Print evaluator values of all evaluators evaluated in the evaluation context.
*/
template<typename Entry>
void print_initial_evaluator_values(const EvaluationContext<Entry> &eval_context) {
    eval_context.get_cache().for_each_evaluator_result(
        [] (const Evaluator *eval, const EvaluationResult &result) {
            if (eval->is_used_for_reporting_minima()) {
                eval->report_value_for_initial_state(result);
            }
        }
        );
}

template<typename Entry>
void collect_preferred_operators(EvaluationContext<Entry> &eval_context,
    Evaluator *preferred_operator_evaluator,
    ordered_set::OrderedSet<OperatorID> &preferred_operators) {
    if (!eval_context.is_evaluator_value_infinite(preferred_operator_evaluator)) {
        for (OperatorID op_id : eval_context.get_preferred_operators(preferred_operator_evaluator)) {
            preferred_operators.insert(op_id);
        }
    }
}

#endif
