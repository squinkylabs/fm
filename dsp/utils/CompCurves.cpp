#include "SplineRenderer.h"
#include "CompCurves.h"

#include <functional>


#include "asserts.h"

CompCurves::xy CompCurves::getGainAtRightInflection(const CompCurves::Recipe& r) {
    assert(r.ratio > 0);
    // on the right side, the straight part after inflection is just
    // output.db = input.db / ratio

    const float topOfKneeDb = r.kneeWidth / 2;
    const float topOfKneeVin = float(AudioMath::gainFromDb(topOfKneeDb));
    const float dbGainAtTopOfKnee = topOfKneeDb / r.ratio;
    const float gainAtTopOfKnee = float(AudioMath::gainFromDb(dbGainAtTopOfKnee));
    xy ret;
    ret.x = topOfKneeVin;
    ret.y = gainAtTopOfKnee;
    return ret;
}

CompCurves::xy CompCurves::getGainAtLeftInflection(const CompCurves::Recipe& r) {
    const float bottomOfKneeDb = -r.kneeWidth / 2;
    const float bottomOfKneeVin = float(AudioMath::gainFromDb(bottomOfKneeDb));
    xy ret;
    ret.x = bottomOfKneeVin;
    ret.y = 1;
    return ret;
}

CompCurves::LookupPtr CompCurves::makeCompGainLookup(const CompCurves::Recipe& r) {
    CompCurves::LookupPtr ret = std::make_shared<NonUniformLookupTableParams<float>>();
    if (r.kneeWidth == 0) {
        auto lastPt = addLeftSideCurve(ret, r);
        addRightSideCurve(ret, r, lastPt);
    } else {
        auto lastPtLeft = addLeftSideCurve(ret, r);

        // todo return points
        addMiddleCurve(ret, r, lastPtLeft);

        auto lastPtRight = getGainAtRightInflection(r);
        addRightSideCurve(ret, r, lastPtRight);
    }
    NonUniformLookupTable<float>::finalize(*ret);
    return ret;
}

CompCurves::xy CompCurves::addLeftSideCurve(LookupPtr ptr, const Recipe& r) {
    const float bottomOfKneeDb = -r.kneeWidth / 2;
    const float bottomOfKneeVin = float(AudioMath::gainFromDb(bottomOfKneeDb));

    NonUniformLookupTable<float>::addPoint(*ptr, 0, 1);
    NonUniformLookupTable<float>::addPoint(*ptr, bottomOfKneeVin, 1);
    return xy(bottomOfKneeVin, bottomOfKneeVin);
}

void CompCurves::addMiddleCurve(LookupPtr table, const Recipe& r, CompCurves::xy init) {
    assert(r.ratio > 1);
    // where we start curve
    const double x0Db = -r.kneeWidth / 2;
    const double x1Db = r.kneeWidth / 2;

    for (double xDb = x0Db; xDb <= x1Db; xDb += 1) {
        const double squareTerm = (xDb + r.kneeWidth / 2);
        const double rTerm = (1.0 / r.ratio) - 1;

        const double yDb = xDb + rTerm * squareTerm * squareTerm / (2 * r.kneeWidth);

        const float x = float(AudioMath::gainFromDb(xDb));
        const float yV = float(AudioMath::gainFromDb(yDb));
        const float gain = yV / x;

        // printf("in middel x=%f ydb=%f v-g=%f, %f\n", xDb, yDb, x, gain);
        NonUniformLookupTable<float>::addPoint(*table, x, gain);
    }

}

void CompCurves::addRightSideCurve(LookupPtr table, const Recipe& r, CompCurves::xy init) {
    assert(r.kneeWidth < 20);  // code below will fail it so
    // start of left curve
    const double x0Db = r.kneeWidth / 2;
    const double dbSlope = 1.0 / r.ratio;

    const double x1Db = AudioMath::db(10);   // let's plot out to +0 db
    const double x2Db = AudioMath::db(100);  // no, let's have a 40 db range!

    // printf("x0 db = %f, x1Db = %f, x2 = %f\n", x0Db, x1Db, x2Db );
    double incrementDb = 1;
    for (double xDb = x0Db; xDb <= x2Db; xDb += incrementDb) {
        const double yDb = dbSlope * xDb;

        const float x = float(AudioMath::gainFromDb(xDb));
        const float gain = float(AudioMath::gainFromDb(yDb)) / x;
        // printf("new R: another right point db=%f,%f v-g=%f,%f\n", xDb, yDb, x, gain);
        NonUniformLookupTable<float>::addPoint(*table, x, gain);

        // for tons of comp, we can be a little less precise.
        incrementDb = xDb > x1Db ? 3 : 1;
    }
}

std::function<double(double)> CompCurves::_getContinuousCurve(const CompCurves::Recipe& r, bool bUseSplines) {
    std::shared_ptr<NonUniformLookupTableParams<double>> splineLookup;
    if (bUseSplines) {
        splineLookup = makeSplineMiddle(r);
    }
    return [r, bUseSplines, splineLookup](double x) {
        if (x < bottomOfKneeVin(r)) {
            return 1.0;  // constant gain of 1 below thresh
        } else if (x < topOfKneeVin(r)) {
            if (!bUseSplines) {
                const double xdb = AudioMath::db(x);
                const double squareTerm = (xdb + r.kneeWidth / 2);
                const double rTerm = (1.0 / r.ratio) - 1;
                const double yDb = xdb + rTerm * squareTerm * squareTerm / (2 * r.kneeWidth);
                const double x2 = x;

                const double yV = AudioMath::gainFromDb(yDb);
                const double gain = yV / x2;
                return gain;
            } else {
                assert(splineLookup);
                return NonUniformLookupTable<double>::lookup(*splineLookup, x);
            }
        } else {
            const double xdb = AudioMath::db(x);
            const double dbSlope = 1.0 / r.ratio;
            const double yDb = dbSlope * xdb;

            const double xTest = AudioMath::gainFromDb(xdb);
            (void) xTest;
            assertClose(x, xTest, .000001);
            const double gain = AudioMath::gainFromDb(yDb) / x;

            return gain;
        }
        return 0.0;
    };
}

CompCurves::CompCurveLookupPtr CompCurves::makeCompGainLookup2(const Recipe& r) {
    return makeCompGainLookupEither(r, false);
}

CompCurves::CompCurveLookupPtr CompCurves::makeCompGainLookup3(const Recipe& r) {
    return makeCompGainLookupEither(r, true);
}

// at 10, distortion jumps at the changes
// 100 it does a little bit? but 1000 is too high...
const int tableSize = 100;
CompCurves::CompCurveLookupPtr CompCurves::makeCompGainLookupEither(const Recipe& r, bool bUseSpline) {

    CompCurveLookupPtr ret = std::make_shared<CompCurveLookup>();

    const float bottomOfKneeDb = -r.kneeWidth / 2;
    ret->bottomOfKneeVin = float(AudioMath::gainFromDb(bottomOfKneeDb));
    ret->dividingLine = 2;      // should this really be gain @6db? don't know...
    auto func = _getContinuousCurve(r, bUseSpline);

    LookupTable<T>::init(ret->lowRange, tableSize, ret->bottomOfKneeVin, ret->dividingLine, func);
    const float delta = 0;          // 1 was whack
    LookupTable<T>::init(ret->highRange, tableSize, ret->dividingLine - delta, 100, func);
    return ret;
}

float CompCurves::CompCurveLookup::lookup(float x) const {
    if (x <= bottomOfKneeVin) {
        return 1;
    }
    if (x < dividingLine) {
        return LookupTable<T>::lookup(lowRange, x, false);
    }
    return LookupTable<T>::lookup(highRange, x, true);
}

std::function<float(float)> CompCurves::getLambda(const Recipe& r, Type t) {
    std::function<float(float)> ret;

    switch (t) {
        case Type::ClassicNU: {
            CompCurves::LookupPtr classicNUPtr = CompCurves::makeCompGainLookup(r);
            ret = [classicNUPtr](float x) {
                return CompCurves::lookup(classicNUPtr, x);
            };
        } break;
        case Type::ClassicLin: {
            CompCurves::CompCurveLookupPtr classicLinPtr = CompCurves::makeCompGainLookup2(r);
            ret = [classicLinPtr](float x) {
                return classicLinPtr->lookup(x);
            };
        } break;
        case Type::SplineLin: {
            CompCurves::CompCurveLookupPtr splineLinPtr = CompCurves::makeCompGainLookup3(r);
            ret = [splineLinPtr](float x) {
                return splineLinPtr->lookup(x);
            };
        } break;
        default:
            assert(false);
    }
    return ret;
}

std::shared_ptr<NonUniformLookupTableParams<double>>
CompCurves::makeSplineMiddle(const Recipe& r) {
    std::shared_ptr<NonUniformLookupTableParams<double>> firstTableParam = std::make_shared<NonUniformLookupTableParams<double>>();
    {
        // Make a hermite spline from the Recipe
        auto spline = HermiteSpline::make(int(std::round(r.ratio)), int(std::round(r.kneeWidth)));
        if (!spline) {
            return nullptr;
        }

        // First make a non-uniform for mapping that hermite with linear axis (wrong)
        int iNum = 1024;
        for (int i = 0; i < iNum; ++i) {
            double t = double(i) / double(iNum);
            auto pt = spline->renderPoint(t);
            NonUniformLookupTable<double>::addPoint(*firstTableParam, pt.first, pt.second);
        }
        NonUniformLookupTable<double>::finalize(*firstTableParam);
    }

    // Render the warped spline into a non-uniform lookup table so we can do cartesian mapping (later)
    // (for now we are only doing non-uniform. Until it works right).
    auto interp = CompCurves::getKneeInterpolator(r);
    std::shared_ptr<NonUniformLookupTableParams<double>> params = std::make_shared<NonUniformLookupTableParams<double>>();
    int iNum2 = 1024;
    //int iNum2 = 12;
    for (int i = 0; i <= iNum2; ++i) {

        // map i continuously into .5 ... 2 (the knee input level in volts)
        double vInVolts = double(i) / double(iNum2);  // 0..1
        vInVolts = interp(vInVolts);

        double vInDb = AudioMath::db(vInVolts);  // -6 ... 6
      
        // our spline lookup give the gain in DB for an input in DB, -6..+6 in
        double yGainDb = NonUniformLookupTable<double>::lookup(*firstTableParam, vInDb);
        double yGainVolts = AudioMath::gainFromDb(yGainDb);

      //  double yOutDebug = yGainVolts * vInVolts;
      
        NonUniformLookupTable<double>::addPoint(*params, vInVolts, yGainVolts);

       //SQINFO("i=%d  vInVolts=%f vInDb =%f", i, vInVolts, vInDb);
       //SQINFO("yGainDb=%f yg=%f debug=%f\n", yGainDb, yGainVolts, yOutDebug);
    }

    //assert(false);
    NonUniformLookupTable<double>::finalize(*params);
    return params;
}


void CompCurves::CompCurveLookup::_dump() const {
    //SQINFO("dumping curve low");
    lowRange._dump();
    //SQINFO("dumping curve high");
    highRange._dump();
    //SQINFO("done dumping curve");
}