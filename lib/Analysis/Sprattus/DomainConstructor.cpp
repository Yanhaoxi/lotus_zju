#include "Analysis/Sprattus/DomainConstructor.h"

#include "Analysis/Sprattus/AbstractValue.h"
#include "Analysis/Sprattus/ResultStore.h"
#include "Analysis/Sprattus/Config.h"
#include "Analysis/Sprattus/ParamStrategy.h"
#include "Analysis/Sprattus/domains/Product.h"

namespace sprattus
{
std::vector<DomainConstructor>* DomainConstructor::KnownDomains_;

DomainConstructor::DomainConstructor(const configparser::Config& config)
{
    *this = config.get<DomainConstructor>("AbstractDomain", "Variant",
                                          DomainConstructor());

    assert(!isInvalid()); // default value is never used in config.get
}

unique_ptr<AbstractValue>
DomainConstructor::makeBottom(const DomainConstructor::args& args) const
{
    DomainConstructor dc = autoParameterize(0);
    return dc.FactoryFunc_(args);
}

unique_ptr<AbstractValue>
DomainConstructor::makeBottom(const FunctionContext& fctx,
                              llvm::BasicBlock* loc, bool after) const
{
    DomainConstructor dc = autoParameterize(0);
    DomainConstructor::args args;
    args.fctx = &fctx;
    args.location = loc;
    args.is_after_bb = after;
    return dc.FactoryFunc_(args);
}

DomainConstructor DomainConstructor::autoParameterize(int desired_arity) const
{
    assert(Arity_ >= desired_arity && desired_arity >= 0);
    DomainConstructor dc = *this;

    while (dc.Arity_ > desired_arity) {
        if (dc.Arity_ >= desired_arity + 2)
            dc = dc.parameterize(ParamStrategy::AllValuePairs());
        else
            dc = dc.parameterize(ParamStrategy::AllValues());
    }

    assert(dc.Arity_ == desired_arity);
    return dc;
}

DomainConstructor
DomainConstructor::parameterize(const ParamStrategy& pstrategy)
{
    factory_func_t factory_func = FactoryFunc_;

    auto f = [pstrategy, factory_func](const DomainConstructor::args& args) {
        auto result = make_unique<domains::Product>(*args.fctx);

        for (auto& pvec : pstrategy.generateParams(args)) {
            DomainConstructor::args local_args = args;

            assert((int)pvec.size() == pstrategy.arity());
            for (Expression& p : pvec) {
                local_args.parameters.push_back(p);
            }

            result->add(factory_func(local_args));
        }

        result->finalize();
        return unique_ptr<AbstractValue>(std::move(result));
    };

    int new_arity = Arity_ - pstrategy.arity();
    assert(new_arity >= 0);
    return DomainConstructor(Name_, Description_, new_arity, f);
}

DomainConstructor
DomainConstructor::product(std::vector<DomainConstructor> doms)
{
    assert(doms.size() > 0);

    // arity of the result is the minimum of arities of components
    int arity = INT_MAX;
    for (auto& d : doms) {
        if (d.arity() < arity)
            arity = d.arity();
    }

    // components with greater arity need to be parameterized with default
    // strategies
    for (auto& d : doms) {
        d = d.autoParameterize(arity);
    }

    auto f = [doms](const DomainConstructor::args& args) {
        auto prod = make_unique<domains::Product>(*args.fctx);
        for (auto& d : doms) {
            prod->add(d.FactoryFunc_(args));
        }
        prod->finalize();
        return unique_ptr<AbstractValue>(std::move(prod));
    };

    // TODO: provide a more informative name based on names of components
    return DomainConstructor("product", "", arity, f);
}

DomainConstructor::DomainConstructor(std::string name, std::string desc,
                                     alt_ffunc_0 factory_func)
    : DomainConstructor(
          name, desc, 0, [factory_func](const DomainConstructor::args& args) {
              return factory_func(*args.fctx, args.location, args.is_after_bb);
          })
{
}

DomainConstructor::DomainConstructor(std::string name, std::string desc,
                                     alt_ffunc_1 factory_func)
    : DomainConstructor(name, desc, 1,
                        [factory_func](const DomainConstructor::args& args) {
                            return factory_func(args.parameters[0], args);
                        })
{
}

DomainConstructor::DomainConstructor(std::string name, std::string desc,
                                     alt_ffunc_2 factory_func)
    : DomainConstructor(
          name, desc, 2, [factory_func](const DomainConstructor::args& args) {
              return factory_func(args.parameters[0], args.parameters[1], args);
          })
{
}
} // namespace sprattus
