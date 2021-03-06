#include "search_engine.h"

#include "evaluation_context.h"
#include "evaluator.h"
#include "option_parser.h"
#include "plugin.h"

#include "evaluators/g_evaluator.h"
#include "task_utils/successor_generator.h"
#include "task_utils/task_properties.h"
#include "tasks/root_task.h"
#include "utils/countdown_timer.h"
#include "utils/logging.h"
#include "utils/rng_options.h"
#include "utils/system.h"
#include "utils/timer.h"

#include <cassert>
#include <iostream>
#include <limits>

using namespace std;
using utils::ExitCode;

class PruningMethod;

successor_generator::SuccessorGenerator &get_successor_generator(const TaskProxy &task_proxy) {
    utils::g_log << "Building successor generator..." << flush;
    int peak_memory_before = utils::get_peak_memory_in_kb();
    utils::Timer successor_generator_timer;
    successor_generator::SuccessorGenerator &successor_generator =
        successor_generator::g_successor_generators[task_proxy];
    successor_generator_timer.stop();
    utils::g_log << "done!" << endl;
    int peak_memory_after = utils::get_peak_memory_in_kb();
    int memory_diff = peak_memory_after - peak_memory_before;
    utils::g_log << "peak memory difference for successor generator creation: "
                 << memory_diff << " KB" << endl
                 << "time for successor generation creation: "
                 << successor_generator_timer << endl;
    return successor_generator;
}

SearchEngine::SearchEngine(const Options &opts)
    : status(IN_PROGRESS),
      solution_found(false),
      task(tasks::g_root_task),
      task_proxy(*task),
      state_registry(task_proxy),
      successor_generator(get_successor_generator(task_proxy)),
      search_space(state_registry),
      search_progress(opts.get<utils::Verbosity>("verbosity")),
      statistics(opts.get<utils::Verbosity>("verbosity")),
      is_unit_cost(task_properties::is_unit_cost(task_proxy)),
      max_time(opts.get<double>("max_time")),
      verbosity(opts.get<utils::Verbosity>("verbosity")) {
    if (opts.get<int>("bound") < 0) {
        cerr << "error: negative cost bound " << opts.get<int>("bound") << endl;
        utils::exit_with(ExitCode::SEARCH_INPUT_ERROR);
    }
    bound = opts.get<int>("bound");
    if (bound != numeric_limits<int>::max()) {
        // TODO: if the g-evaluator is present and uses the original costs, we
        // would like to reuse it as the real-g evaluator, but it's tricky to
        // test whether this situation applies.
        Options real_g_eval_opts;
        real_g_eval_opts.set<shared_ptr<AbstractTask>>("transform", tasks::g_root_task);
        real_g_eval_opts.set<bool>("cache_estimates", true);
        real_g_evaluator = make_shared<g_evaluator::GEvaluator>(real_g_eval_opts);
    }
    if (real_g_evaluator && !real_g_evaluator->does_cache_estimates()) {
        cerr << "real_g_evaluator must cache its estimates" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
    task_properties::print_variable_statistics(task_proxy);
}

SearchEngine::~SearchEngine() {
}

bool SearchEngine::found_solution() const {
    return solution_found;
}

SearchStatus SearchEngine::get_status() const {
    return status;
}

const Plan &SearchEngine::get_plan() const {
    assert(solution_found);
    return plan;
}

void SearchEngine::set_plan(const Plan &p) {
    solution_found = true;
    plan = p;
}

void SearchEngine::search() {
    initialize();
    utils::CountdownTimer timer(max_time);
    while (status == IN_PROGRESS) {
        status = step();
        if (timer.is_expired()) {
            utils::g_log << "Time limit reached. Abort search." << endl;
            status = TIMEOUT;
            break;
        }
    }
    // TODO: Revise when and which search times are logged.
    utils::g_log << "Actual search time: " << timer.get_elapsed_time() << endl;
}

bool SearchEngine::check_goal_and_set_plan(const State &state) {
    if (task_properties::is_goal_state(task_proxy, state)) {
        utils::g_log << "Solution found!" << endl;
        Plan plan;
        search_space.trace_path(state, plan);
        set_plan(plan);
        return true;
    }
    return false;
}

void SearchEngine::save_plan_if_necessary() {
    if (found_solution()) {
        plan_manager.save_plan(get_plan(), task_proxy);
    }
}

/* TODO: merge this into add_options_to_parser when all search
         engines support pruning.

   Method doesn't belong here because it's only useful for certain derived classes.
   TODO: Figure out where it belongs and move it there. */
void SearchEngine::add_pruning_option(OptionParser &parser) {
    parser.add_option<shared_ptr<PruningMethod>>(
        "pruning",
        "Pruning methods can prune or reorder the set of applicable operators in "
        "each state and thereby influence the number and order of successor states "
        "that are considered.",
        "null()");
}

void SearchEngine::add_options_to_parser(OptionParser &parser) {
    parser.add_option<int>(
        "bound",
        "exclusive depth bound on g-values. Cutoffs are always performed according to "
        "the real path cost, regardless of the cost type used by the optional g-evaluator",
        "infinity");
    parser.add_option<double>(
        "max_time",
        "maximum time in seconds the search is allowed to run for. The "
        "timeout is only checked after each complete search step "
        "(usually a node expansion), so the actual runtime can be arbitrarily "
        "longer. Therefore, this parameter should not be used for time-limiting "
        "experiments. Timed-out searches are treated as failed searches, "
        "just like incomplete search algorithms that exhaust their search space.",
        "infinity");
    utils::add_verbosity_option_to_parser(parser);
}

/* Method doesn't belong here because it's only useful for certain derived classes.
   TODO: Figure out where it belongs and move it there. */
void SearchEngine::add_succ_order_options(OptionParser &parser) {
    vector<string> options;
    parser.add_option<bool>(
        "randomize_successors",
        "randomize the order in which successors are generated",
        "false");
    parser.add_option<bool>(
        "preferred_successors_first",
        "consider preferred operators first",
        "false");
    parser.document_note(
        "Successor ordering",
        "When using randomize_successors=true and "
        "preferred_successors_first=true, randomization happens before "
        "preferred operators are moved to the front.");
    utils::add_rng_options(parser);
}

static PluginTypePlugin<SearchEngine> _type_plugin(
    "SearchEngine",
    // TODO: Replace empty string by synopsis for the wiki page.
    "");
