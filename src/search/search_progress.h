#ifndef SEARCH_PROGRESS_H
#define SEARCH_PROGRESS_H

#include <unordered_map>


#include "evaluation_context.h"
#include "evaluator.h"

#include "../utils/logging.h"

template<typename Entry>
class EvaluationContext;
class Evaluator;

namespace utils {
enum class Verbosity;
}

/*
  This class helps track search progress.

  Evaluators can be configured to be used for reporting new minima, boosting
  open lists, or both. This class maintains a record of minimum evaluator
  values for evaluators that are used for either of these two things.
*/


class SearchProgress {
    const utils::Verbosity verbosity;
    std::unordered_map<const Evaluator *, int> min_values;

    bool process_evaluator_value(const Evaluator *evaluator, int value);

public:
    explicit SearchProgress(utils::Verbosity verbosity);
    ~SearchProgress() = default;

    /*
      Call the following function after each state evaluation.

      It returns true if the evaluation context contains a new minimum value
      for at least one evaluator used for boosting.

      It also prints one line of output for all evaluators used for reporting
      minima that have a new minimum value in the given evaluation context.

      In both cases this includes the situation where the evaluator in question
      has not been evaluated previously, e.g., after evaluating the initial
      state.
    */
    template<class Entry>
    bool check_progress(const EvaluationContext<Entry> &eval_context);
};

template<class Entry>
bool SearchProgress::check_progress(const EvaluationContext<Entry> &eval_context) {
    bool boost = false;
    eval_context.get_cache().for_each_evaluator_result(
        [this, &boost](const Evaluator *eval, const EvaluationResult &result) {
            if (eval->is_used_for_reporting_minima() || eval->is_used_for_boosting()) {
                if (process_evaluator_value(eval, result.get_evaluator_value())) {
                    if (verbosity >= utils::Verbosity::NORMAL &&
                        eval->is_used_for_reporting_minima()) {
                        eval->report_new_minimum_value(result);
                    }
                    if (eval->is_used_for_boosting()) {
                        boost = true;
                    }
                }
            }
        }
        );
    return boost;
}

#endif
