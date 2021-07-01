#ifndef EVALUATION_CONTEXT_H
#define EVALUATION_CONTEXT_H

#include "evaluation_result.h"
#include "evaluator.h"
#include "evaluator_cache.h"
#include "operator_id.h"
#include "search_statistics.h"
#include "state_registry.h"
#include "task_proxy.h"

#include <unordered_map>


class Evaluator;
class SearchStatistics;

/*
  TODO: Now that we have an explicit EvaluationResult class, it's
  perhaps not such a great idea to duplicate all its access methods
  like "get_evaluator_value()" etc. on EvaluationContext. Might be a
  simpler interface to just give EvaluationContext an operator[]
  method or other simple way of accessing a given EvaluationResult
  and then use the methods of the result directly.
*/

/*
  EvaluationContext has two main purposes:

  1. It packages up the information that evaluators and open lists
     need in order to perform an evaluation: the state and whether it was
     reached by a preferred operator.

  2. It caches computed evaluator values and preferred operators for
     the current evaluation so that they do not need to be computed
     multiple times just because they appear in multiple contexts,
     and also so that we don't need to know a priori which evaluators
     need to be evaluated throughout the evaluation process.

     For example, our current implementation of A* search uses the
     evaluator value h at least three times: twice for its
     tie-breaking open list based on <g + h, h> and a third time for
     its "progress evaluator" that produces output whenever we reach a
     new best f value.
*/

// TODO: are there reasons against having state living here?
// Would a shared_ptr be better (avoid duplicate States over copied contexts)
template<typename Entry>
class EvaluationContext {
    EvaluatorCache cache;
    State state;
    OperatorID operator_id;
    bool preferred;
    SearchStatistics *statistics;
    bool calculate_preferred;

    EvaluationContext(
        const EvaluatorCache &cache, State state,
        OperatorID operator_id, bool is_preferred,
        SearchStatistics *statistics, bool calculate_preferred);
public:
    /*
      Copy existing heuristic cache and use it to look up heuristic values.
      Used for example by lazy search.

      TODO: Can we reuse caches? Can we move them instead of copying them?
    */
    EvaluationContext(
        const EvaluationContext &other, bool is_preferred,
        SearchStatistics *statistics, bool calculate_preferred = false);
    /*
      Create new heuristic cache for caching heuristic values. Used for example
      by eager search.
    */
    EvaluationContext(
        const Entry &entry, const StateRegistry &registry,
        bool is_preferred, SearchStatistics *statistics,
        bool calculate_preferred = false);
    /*
      Use the following constructor when you don't care about preferredness
      (and statistics), e.g., when sampling states for heuristics.

      TODO: In the long term we might want to separate how path-dependent and
            path-independent evaluators are evaluated. This change would remove
            the need to store the preferredness for evaluation contexts that
            don't need this information.
    */
    EvaluationContext(
        const Entry &entry, const StateRegistry &registry,
        SearchStatistics *statistics = nullptr, bool calculate_preferred = false);

    const EvaluationResult &get_result(Evaluator *eval);
    const EvaluatorCache &get_cache() const;
    // TODO: it's a bit hacky but I will use this for both types of entry
    const State &get_state() const;
    OperatorID get_operator_id () const;
    bool is_preferred() const;

    /*
      Use get_evaluator_value() to query finite evaluator values. It
      is an error (guarded by an assertion) to call this method for
      states with infinite evaluator values, because such states often
      need to be treated specially and we want to catch cases where we
      forget to do this.

      In cases where finite and infinite evaluator values can be
      treated uniformly, use get_evaluator_value_or_infinity(), which
      returns numeric_limits<int>::max() for infinite estimates.
    */
    bool is_evaluator_value_infinite(Evaluator *eval);
    int get_evaluator_value(Evaluator *eval);
    int get_evaluator_value_or_infinity(Evaluator *eval);
    const std::vector<OperatorID> &get_preferred_operators(Evaluator *eval);
    bool get_calculate_preferred() const;
};


using StateEvaluationContextEntry = StateID;
using EdgeEvaluationContextEntry = std::pair<StateID, OperatorID>;

using StateEvaluationContext = EvaluationContext<StateEvaluationContextEntry>;
using EdgeEvaluationContext = EvaluationContext<EdgeEvaluationContextEntry>;


using namespace std;

template<typename Entry>
inline EvaluationContext<Entry>::EvaluationContext(
    const EvaluatorCache &cache, State state,
    OperatorID operator_id, bool is_preferred,
    SearchStatistics *statistics, bool calculate_preferred)
    : cache(cache),
      state(state),
      operator_id(operator_id),
      preferred(is_preferred),
      statistics(statistics),
      calculate_preferred(calculate_preferred) {
}


template<typename Entry>
inline EvaluationContext<Entry>::EvaluationContext(
    const EvaluationContext<Entry> &other, bool is_preferred,
    SearchStatistics *statistics, bool calculate_preferred)
    : EvaluationContext(
          other.cache, other.state, other.operator_id, is_preferred,
          statistics, calculate_preferred) {
}

template<>
inline StateEvaluationContext::EvaluationContext(
    const StateID &entry, const StateRegistry &registry, bool is_preferred,
    SearchStatistics *statistics, bool calculate_preferred)
    : StateEvaluationContext(
          EvaluatorCache(), registry.lookup_state(entry),
          OperatorID::no_operator, is_preferred, statistics,
          calculate_preferred) {
}

template<>
inline EdgeEvaluationContext::EvaluationContext(
    const pair<StateID, OperatorID> &entry, const StateRegistry &registry,
    bool is_preferred, SearchStatistics *statistics,
    bool calculate_preferred)
    : EdgeEvaluationContext(
          EvaluatorCache(), registry.lookup_state(entry.first),
          entry.second, is_preferred, statistics,
          calculate_preferred) {
}

template<>
inline StateEvaluationContext::EvaluationContext(
    const StateID &entry, const StateRegistry &registry,
    SearchStatistics *statistics, bool calculate_preferred)
    : EvaluationContext(
          EvaluatorCache(), registry.lookup_state(entry),
          OperatorID::no_operator, false, statistics, calculate_preferred) {
}

template<>
inline EdgeEvaluationContext::EvaluationContext(
    const pair<StateID, OperatorID> &entry,
    const StateRegistry &registry, SearchStatistics *statistics,
    bool calculate_preferred)
    : EdgeEvaluationContext(
          EvaluatorCache(), registry.lookup_state(entry.first),
          entry.second, false, statistics, calculate_preferred) {
}

template<typename Entry>
inline const EvaluationResult &EvaluationContext<Entry>::get_result(Evaluator *evaluator) {
    EvaluationResult &result = cache[evaluator];
    if (result.is_uninitialized()) {
        result = evaluator->compute_result(*this);
        if (statistics &&
            evaluator->is_used_for_counting_evaluations() &&
            result.get_count_evaluation()) {
            statistics->inc_evaluations();
        }
    }
    return result;
}

template<typename Entry>
inline const EvaluatorCache &EvaluationContext<Entry>::get_cache() const {
    return cache;
}

template<typename Entry>
inline const State &EvaluationContext<Entry>::get_state() const {
    return state;
}

// TODO: exit?
template<>
inline OperatorID StateEvaluationContext::get_operator_id() const {
    cerr << "StateEvaluationContext does not have an operator ID" << endl;
    return operator_id;
}

template<>
inline OperatorID EdgeEvaluationContext::get_operator_id() const {
    return operator_id;
}

template<typename Entry>
inline bool EvaluationContext<Entry>::is_preferred() const {
    return preferred;
}

template<typename Entry>
inline bool EvaluationContext<Entry>::is_evaluator_value_infinite(Evaluator *eval) {
    return get_result(eval).is_infinite();
}

template<typename Entry>
inline int EvaluationContext<Entry>::get_evaluator_value(Evaluator *eval) {
    int h = get_result(eval).get_evaluator_value();
    assert(h != EvaluationResult::INFTY);
    return h;
}

template<typename Entry>
inline int EvaluationContext<Entry>::get_evaluator_value_or_infinity(Evaluator *eval) {
    return get_result(eval).get_evaluator_value();
}

template<typename Entry>
inline const vector<OperatorID> &
EvaluationContext<Entry>::get_preferred_operators(Evaluator *eval) {
    return get_result(eval).get_preferred_operators();
}


template<typename Entry>
inline bool EvaluationContext<Entry>::get_calculate_preferred() const {
    return calculate_preferred;
}



#endif
