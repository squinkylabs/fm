
#include "SParse.h"

#include <assert.h>

#include "FilePath.h"
#include "LexContext.h"
#include "SInstrument.h"
#include "SLex.h"
#include "SqLog.h"
#include "SqStream.h"
#include "share/windows_unicode_filenames.h"

// global for mem leak detection
int parseCount = 0;

#if defined(ARCH_WIN)

FILE* SParse::openFile(const FilePath& fp) {
    flac_set_utf8_filenames(true);
    return flac_internal_fopen_utf8(fp.toString().c_str(), "r");
}

#else

FILE* SParse::openFile(const FilePath& fp) {
    return fopen(fp.toString().c_str(), "r");
}

#endif

std::string SParse::readFileIntoString(FILE* fp) {
    if (fseek(fp, 0, SEEK_END) < 0)
        return "";

    const long size = ftell(fp);
    if (size < 0)
        return "";

    if (fseek(fp, 0, SEEK_SET) < 0)
        return "";

    std::string res;
    res.resize(size);

    const size_t numRead = fread(const_cast<char*>(res.data()), 1, size, fp);
    if (numRead != size_t(size)) {
        res.resize(numRead);
    }
    return res;
}

std::string SParse::goFile(const FilePath& filePath, SInstrumentPtr inst) {
    FILE* fp = openFile(filePath);
    if (!fp) {
        return "can't open " + filePath.toString();
    }
    std::string sContent = readFileIntoString(fp);
    fclose(fp);
    return goCommon(sContent, inst, filePath);
}

std::string SParse::go(const std::string& s, SInstrumentPtr inst) {
    return goCommon(s, inst, FilePath());
}

static std::string filter(const std::string& sInput) {
    std::string ret;
    for (char c : sInput) {
        if (c != '\r') {
            ret.push_back(c);
        }
    }
    return ret;
}

std::string SParse::goCommon(const std::string& sContentIn, SInstrumentPtr outParsedInstrument, const FilePath& fullPathToSFZ) {
    std::string sContent = filter(sContentIn);
    LexContextPtr lexContext = std::make_shared<LexContext>(sContent);
    if (!fullPathToSFZ.empty()) {
        lexContext->addRootPath(fullPathToSFZ);
    }
    SLexPtr lex = SLex::go(lexContext);
    if (!lex) {
        std::string sError = lexContext->errorString();
        assert(!sError.empty());
        return sError;
    }

    std::string sError = matchHeadingGroups(outParsedInstrument, lex);
    if (!sError.empty()) {
        return sError;
    }
    if (lex->next() != nullptr) {
        auto item = lex->next();
        auto type = item->itemType;
        auto lineNumber = item->lineNumber;
        SqStream errorStream;
        errorStream.add("extra tok line number ");
        errorStream.add(int(lineNumber));
        errorStream.add(" type= ");
        errorStream.add(int(type));
        errorStream.add(" index=");
        errorStream.add(lex->_index());
        if (type == SLexItem::Type::Tag) {
            auto tag = std::static_pointer_cast<SLexTag>(item);
            //SQINFO("extra tok = %s", tag->tagName.c_str());
        }

        if (type == SLexItem::Type::Identifier) {
            SLexIdentifier* id = static_cast<SLexIdentifier*>(item.get());
            errorStream.add(" id name is ");
            errorStream.add(id->idName);
        }
        return errorStream.str();
    }

    if (outParsedInstrument->headings.empty()) {
        return "no groups or regions";
    }

    return sError;
}

std::string SParse::matchHeadingGroups(SInstrumentPtr inst, SLexPtr lex) {
    for (bool done = false; !done;) {
        auto result = matchHeadingGroup(inst, lex);
        if (result.res == Result::error) {
            return result.errorMessage;
        }
        done = result.res == Result::no_match;
    }
    return "";
}

// I think now this just needs to match a single heading

SParse::Result SParse::matchHeadingGroup(SInstrumentPtr inst, SLexPtr lex) {
    SHeadingPtr theHeading;
    //SQINFO("about to call match extln=%d", lex->next()->lineNumber);
    Result result = matchSingleHeading(lex, theHeading);
    if (result.res == Result::ok && theHeading) {
        inst->headings.push_back(theHeading);
    }
    return result;
}

static std::map<std::string, SHeading::Type> headingTags = {
    {"region", SHeading::Type::Region},
    {"group", SHeading::Type::Group},
    {"global", SHeading::Type::Global},
    {"control", SHeading::Type::Control},
    {"master", SHeading::Type::Master},
    {"curve", SHeading::Type::Curve},
    {"effect", SHeading::Type::Effect},
    {"midi", SHeading::Type::Midi},
    {"sample", SHeading::Type::Sample}};

SHeading::Type getHeadingType(const std::string& s) {
    auto it = headingTags.find(s);
    if (it == headingTags.end()) {
        return SHeading::Type::Unknown;
    }
    return it->second;
}

SParse::Result SParse::matchSingleHeading(SLexPtr lex, SHeadingPtr& outputHeading) {
    Result result;
    auto tok = lex->next();
    if (!tok) {
        result.res = Result::no_match;
        return result;
    }

    const int startLineNumber = lex->next() ? lex->next()->lineNumber : 0;
    SHeading::Type headingType = getHeadingType(getTagName(tok));
    if (headingType == SHeading::Type::Unknown) {
        result.res = Result::no_match;
        return result;
    }

    // ok, here we matched a heading. Remember the name
    // and consume the [heading] token.
    const std::string tagName = getTagName(tok);
    lex->consume();

    // now extract out all the keys and values for this heading
    SKeyValueList keysAndValues;
    std::string s = matchKeyValuePairs(keysAndValues, lex);

    if (!s.empty()) {
        result.res = Result::Res::error;
        result.errorMessage = s;
        return result;
    }

    outputHeading = std::make_shared<SHeading>(headingType, startLineNumber);
    outputHeading->values = std::move(keysAndValues);
    return result;
}

std::string SParse::matchKeyValuePairs(SKeyValueList& values, SLexPtr lex) {
    for (bool done = false; !done;) {
        auto result = matchKeyValuePair(values, lex);
        if (result.res == Result::error) {
            return result.errorMessage;
        }
        done = result.res == Result::no_match;
    }

    return "";
}

SParse::Result SParse::matchKeyValuePair(SKeyValueList& values, SLexPtr lex) {
    auto keyToken = lex->next();
    Result result;

    // if all done, or no more pairs, then leave
    if (!keyToken || (keyToken->itemType != SLexItem::Type::Identifier)) {
        result.res = Result::no_match;
        return result;
    }

    SLexIdentifier* pid = static_cast<SLexIdentifier*>(keyToken.get());
    SKeyValuePairPtr thePair = std::make_shared<SKeyValuePair>();
    thePair->key = pid->idName;
    lex->consume();

    keyToken = lex->next();
    if (!keyToken) {
        result.errorMessage = "unexpected end of tokens";
        result.res = Result::error;
        return result;
    }
    if (keyToken->itemType != SLexItem::Type::Equal) {
        result.errorMessage = "in key=value missing equal sign at file line# " + keyToken->lineNumberAsString();
        result.res = Result::error;
        return result;
    }
    lex->consume();

    keyToken = lex->next();
    if (keyToken->itemType != SLexItem::Type::Identifier) {
        result.errorMessage = "value in key=value is not id. key=" + thePair->key + " line# " + keyToken->lineNumberAsString();
        result.res = Result::error;
        return result;
    }
    lex->consume();
    pid = static_cast<SLexIdentifier*>(keyToken.get());
    thePair->value = pid->idName;

    values.push_back(thePair);
    return result;
}

std::string SParse::getTagName(SLexItemPtr item) {
    // maybe shouldn't call this with null ptr??
    if (!item) {
        return "";
    }
    if (item->itemType != SLexItem::Type::Tag) {
        return "";
    }
    SLexTag* tag = static_cast<SLexTag*>(item.get());
    return tag->tagName;
}
