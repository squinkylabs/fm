
#pragma once

#include <assert.h>

#include <memory>

#include "IComposite.h"

namespace rack {
namespace engine {
struct Module;
}
}  // namespace rack
using Module = ::rack::engine::Module;

template <class TBase>
class BlankDescription : public IComposite {
public:
    Config getParamValue(int i) override;
    int getNumParams() override;
};

template <class TBase>
class Blank : public TBase {
public:
    Blank(Module* module) : TBase(module) {
    }
    Blank() : TBase() {
    }

    /**
    * re-calc everything that changes with sample
    * rate. Also everything that depends on baseFrequency.
    *
    * Only needs to be called once.
    */
    void init();

    enum ParamIds {
        TEST_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        NUM_INPUTS
    };

    enum OutputIds {
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    /** Implement IComposite
     */
    static std::shared_ptr<IComposite> getDescription() {
        return std::make_shared<BlankDescription<TBase>>();
    }

    /**
     * Main processing entry point. Called every sample
     */
    //void step() override;
    void process(const typename TBase::ProcessArgs& args) override;

private:
};

template <class TBase>
inline void Blank<TBase>::init() {
}

template <class TBase>
inline void Blank<TBase>::process(const typename TBase::ProcessArgs& args) {
}

template <class TBase>
int BlankDescription<TBase>::getNumParams() {
    return Blank<TBase>::NUM_PARAMS;
}

template <class TBase>
inline IComposite::Config BlankDescription<TBase>::getParamValue(int i) {
    Config ret(0, 1, 0, "");
    switch (i) {
        case Blank<TBase>::TEST_PARAM:
            ret = {-1.0f, 1.0f, 0, "Test"};
            break;
        default:
            assert(false);
    }
    return ret;
}
