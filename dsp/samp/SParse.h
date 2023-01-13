
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "SamplerSchema.h"
#include "SqLog.h"

class SLex;
class SLexItem;
class SInstrument;
using SLexPtr = std::shared_ptr<SLex>;
using SLexItemPtr = std::shared_ptr<SLexItem>;
using SInstrumentPtr = std::shared_ptr<SInstrument>;

extern int parseCount;
class FilePath;

//---------------------------------------------
class SKeyValuePair {
public:
    SKeyValuePair(const std::string& k, const std::string& v) : key(k), value(v) { ++parseCount; }
    SKeyValuePair() { ++parseCount; }
    ~SKeyValuePair() { --parseCount; }
    std::string key;
    std::string value;
};
using SKeyValuePairPtr = std::shared_ptr<SKeyValuePair>;
using SKeyValueList = std::vector<SKeyValuePairPtr>;

//-----------------------------------------------------
// A heading represents any heading, including regions and groups
class SHeading {
public:
    void _dump() const { 
        //SQINFO("dump heading from line %d", lineNumber+1);
        dumpKeysAndValues(values); }
    enum class Type {
        Region,
        Group,
        Global,
        Control,
        Master,
        // This is kind of a hack, but...
        // All headings are sort of treated the same, but not all of
        // them are inherited by regions. So ATM, all of the types above
        // DO participate in inheritance, and the ones below do not.
        NUM_TYPES_INHERIT = Master + 1,
        Curve,
        Effect,
        Midi,
        Sample,
        Unknown,
        NUM_TYPES_ALL
    };

    SHeading() = delete;
    SHeading(const SHeading&) = delete;
    SHeading(Type t, int lnNumber) : lineNumber(lnNumber), type(t) {
    }
    /**
     * Parsing populates values with the opcodes found while parsing
     */
    SKeyValueList values;

    /**
     * A step in compiling is turning values into compiledValues.
     * This is fairly mechanical, and driven from SamperSchema
     */
    SamplerSchema::KeysAndValuesPtr compiledValues;
    static void dumpKeysAndValues(const SKeyValueList& v) {
        //SQINFO("-- keys and vals:");
        for (auto k : v) {
            //SQINFO("key=%s val=%s", k->key.c_str(), k->value.c_str());
        }
    }
const int lineNumber = 0;
    const Type type = {Type::Unknown};
};

using SHeadingPtr = std::shared_ptr<SHeading>;
using SHeadingList = std::vector<SHeadingPtr>;

//-------------------------------------------
class SParse {
public:
    static std::string go(const std::string& s, SInstrumentPtr);
    static std::string goFile(const FilePath& filePath, SInstrumentPtr);

private:
    static FILE* openFile(const FilePath& fp);
    static std::string readFileIntoString(FILE* fp);
    static std::string goCommon(const std::string& sContent, SInstrumentPtr outParsedInstrument, const FilePath& fullPathToSFZ);

    class Result {
    public:
        std::string errorMessage;
        enum Res {
            ok,        // matched
            no_match,  // finished matching
            error
        };
        Res res = Res::ok;
    };

    // a"heading group" is a series of headings, where a region is also a heading
    static std::string matchHeadingGroups(SInstrumentPtr, SLexPtr);
    static Result matchHeadingGroup(SInstrumentPtr, SLexPtr);

    static Result matchSingleHeading(SLexPtr lex, SHeadingPtr& outputHeading);
    static std::string matchKeyValuePairs(SKeyValueList&, SLexPtr);
    static Result matchKeyValuePair(SKeyValueList&, SLexPtr);

    // return empty if it's not a tag
    static std::string getTagName(SLexItemPtr);
};