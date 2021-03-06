#include "g_evaluator.h"

#include "../evaluation_context.h"
#include "../option_parser.h"
#include "../plugin.h"

using namespace std;

namespace g_evaluator {
GEvaluator::GEvaluator(const Options &opts)
    : Heuristic(opts) {
}

int GEvaluator::compute_heuristic(const State &ancestor_state) {
    // No need to convert the state since we only allow cost transformations.
    return heuristic_cache[ancestor_state].h;
}


EvaluationResult GEvaluator::compute_result(EdgeEvaluationContext &eval_context) {
    EvaluationResult result;

    const State &parent_state = eval_context.get_state();
    OperatorID operator_id = eval_context.get_operator_id();
    int value = heuristic_cache[parent_state].h;
    if (operator_id == OperatorID::no_operator) {
        // remove dirty flag
        heuristic_cache[parent_state].dirty = false;
        result.set_count_evaluation(false);
    } else {
        value += task_proxy.get_operators()[operator_id.get_index()].get_cost();
        // TODO: should we count this evaluation?
        result.set_count_evaluation(true);
    }
    result.set_evaluator_value(value);
    return result;
}
void GEvaluator::get_path_dependent_evaluators(std::set<Evaluator *> &evals) {
    evals.insert(this);
}

void GEvaluator::notify_initial_state(const State &initial_state) {
    heuristic_cache[initial_state].h = 0;
    heuristic_cache[initial_state].dirty = true;
}

void GEvaluator::notify_state_transition(
    const State &parent_state, OperatorID op_id, const State &state) {
    int parent_g = heuristic_cache[parent_state].h;
    assert(parent_g >= 0);
    int old_g = heuristic_cache[state].h;
    assert(old_g == NO_VALUE || old_g >= 0);
    int new_g = parent_g + task_proxy.get_operators()[op_id.get_index()].get_cost();
    if (old_g == NO_VALUE || new_g < old_g) {
        heuristic_cache[state].h = new_g;
        heuristic_cache[state].dirty = true;
    }
}

static shared_ptr<Evaluator> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "g-value evaluator",
        "Returns the g-value (path cost) of the search node.");
    parser.add_option<shared_ptr<AbstractTask>>(
        "transform",
        "Optional task transformation. "
        "Currently, adapt_costs() and no_transform() are available.",
        "no_transform()");
    Options opts = parser.parse();
    opts.set<bool>("cache_estimates", true);
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<GEvaluator>(opts);
}

static Plugin<Evaluator> _plugin("g", _parse, "evaluators_basic");
}
