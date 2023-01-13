#ifndef GENERATIVETRIGGERGENERATOR
#define GENERATIVETRIGGERGENERATOR

#error dont include gtg.h

#include "StochasticGrammar.h"
#include "TriggerSequencer.h"

#include "SqLog.h"


/* Knows how to generate trigger sequence data
 * when evaluating a grammar
 */
class GTGEvaluator : public ProductionRule::EvaluationState
{
public:
    GTGEvaluator(AudioMath::RandomUniformFunc xr, TriggerSequencer::Event * buf) :
        ProductionRule::EvaluationState(xr),
        _buf(buf)
    {
    }

    void writeSymbol(GKEY key) override
    {
        // first: write out a trigger at "current delay"
        _buf->evt = TriggerSequencer::TRIGGER;
        _buf->delayPPQ = _delayPPQ;
        ++_buf;

        // then set current delay to duration of key
        _delayPPQ = ProductionRuleKeys::getDuration(key);
    }

    // call this to write final event
    void writeEnd()
    {
        _buf->evt = TriggerSequencer::END;
        _buf->delayPPQ = _delayPPQ;
    }
private:
    TriggerSequencer::Event * _buf;
    int _delayPPQ = 0;
};


/* wraps up some stochastic gnerative grammar stuff feeding
 * a trigger sequencer
 */
class GenerativeTriggerGenerator
{
public:
    GenerativeTriggerGenerator(AudioMath::RandomUniformFunc r, const ProductionRule * rules, int numRules, GKEY initialState) :
        _r(r),
        _rules(rules),
        _numRules(numRules),
        _initKey(initialState)
    {
        _data[0].delayPPQ = 0;
        _data[0].evt = TriggerSequencer::END;
        _seq = new TriggerSequencer(_data);
    }
    ~GenerativeTriggerGenerator()
    {
        delete _seq;
    }


    void setGrammar(const ProductionRule * rules, int numRules, GKEY initialState)
    {
        _rules = rules;
        _numRules = numRules;
        _initKey = initialState;
    }

    // returns true if trigger generated
#if 0
    bool clock()
    {
        _seq->clock();
        bool ret = _seq->getTrigger();
        if (_seq->getEnd()) {
            // when we finish playing the seq, generate a new random one
            generate();
            ret |= _seq->getTrigger();
            //printf("this should be getTrigger!!!\n");
        }
        //SQINFO("GTG clock ret %d", ret);
        return ret;
    }
#endif
private:
    TriggerSequencer * _seq;
    TriggerSequencer::Event _data[33];
    AudioMath::RandomUniformFunc _r;
    const ProductionRule * _rules;
    int _numRules;
    GKEY _initKey;
    //
    void generate()
    {
        //SQINFO("gtg generate rules=%p", _rules);
        GTGEvaluator es(_r, _data);
        es.rules = _rules;
        es.numRules = _numRules;
        ProductionRule::evaluate(es, _initKey);

        es.writeEnd();
        TriggerSequencer::isValid(_data);
        _seq->reset(_data);
        assert(!_seq->getEnd());
        //SQINFO("gtg generate exit");
#if 0
        printf("just generated trigger seq\n");
        TriggerSequencer::Event * p;
        for (p = _data; p->evt != TriggerSequencer::END; ++p) {
            printf("evt=%d, delay=%d\n", p->evt, p->delay);
        }
        printf("delay to end = %d\n", p->delay);
#endif
    }
};
#endif