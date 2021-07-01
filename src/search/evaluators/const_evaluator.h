#ifndef EVALUATORS_CONST_EVALUATOR_H
#define EVALUATORS_CONST_EVALUATOR_H

#include "../evaluator.h"

namespace options {
class Options;
}

namespace const_evaluator {
class ConstEvaluator : public Evaluator {
    int value;

protected:
    virtual EvaluationResult compute_result(StateEvaluationContext &eval_context) override;
    virtual EvaluationResult compute_result(EdgeEvaluationContext &eval_context) override;

public:
    explicit ConstEvaluator(const options::Options &opts);
    virtual void get_path_dependent_evaluators(
        std::set<Evaluator *> &) override {}
    virtual ~ConstEvaluator() override = default;
};
}

#endif
