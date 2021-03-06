#ifndef EVALUATORS_WEIGHTED_EVALUATOR_H
#define EVALUATORS_WEIGHTED_EVALUATOR_H

#include "../evaluator.h"

#include <memory>

namespace options {
class Options;
}

namespace weighted_evaluator {
class WeightedEvaluator : public Evaluator {
    std::shared_ptr<Evaluator> evaluator;
    int w;

public:
    explicit WeightedEvaluator(const options::Options &opts);
    WeightedEvaluator(const std::shared_ptr<Evaluator> &eval, int weight);
    virtual ~WeightedEvaluator() override;

    virtual bool dead_ends_are_reliable() const override;
    virtual EvaluationResult compute_result(
        StateEvaluationContext &eval_context) override;
    virtual EvaluationResult compute_result(
        EdgeEvaluationContext &eval_context) override;
    virtual void get_path_dependent_evaluators(std::set<Evaluator *> &evals) override;
};
}

#endif
