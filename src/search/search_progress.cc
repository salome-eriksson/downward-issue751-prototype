#include "search_progress.h"

#include "evaluation_context.h"
#include "evaluator.h"

#include "../utils/logging.h"

#include <iostream>
#include <string>
using namespace std;


SearchProgress::SearchProgress(utils::Verbosity verbosity)
    : verbosity(verbosity) {
}

bool SearchProgress::process_evaluator_value(const Evaluator *evaluator, int value) {
    /*
      Handle one evaluator value:
      1. insert into or update min_values if necessary
      2. return true if this is a new lowest value
         (includes case where we haven't seen this evaluator before)
    */
    auto insert_result = min_values.insert(make_pair(evaluator, value));
    auto iter = insert_result.first;
    bool was_inserted = insert_result.second;
    if (was_inserted) {
        // We haven't seen this evaluator before.
        return true;
    } else {
        int &min_value = iter->second;
        if (value < min_value) {
            min_value = value;
            return true;
        }
    }
    return false;
}
