
#include "SLex.h"

#include <assert.h>

#include <fstream>

#include "FilePath.h"
#include "LexContext.h"
#include "SqLog.h"
#include "SqStream.h"

SLex::SLex(LexContextPtr ctx) : context(ctx) {
}

SLexPtr SLex::go(const std::string& sContent) {
    LexContextPtr ctx = std::make_shared<LexContext>(sContent);
    return go(ctx);
}

SLexPtr SLex::go(LexContextPtr context) {
    SLex* lx = new SLex(context);
    return goCommon(lx, context);
}

SLexPtr SLex::goRecurse(LexContextPtr context) {
    SLex* lx = new SLex(context);
    return goCommon(lx, context);
}

#if 0
char SLex::getNextChar() {
    const char nextC = (i >= (sContent.size() - 1)) ? -1 : sContent[i + 1];
}
#endif

SLexPtr SLex::goCommon(SLex* lx, LexContextPtr ctx) {
    int count = 0;
    SLexPtr result(lx);

    std::string sContent = ctx->getCurrentContent();
    for (size_t i = 0; i < sContent.size(); ++i) {
        const char c = sContent[i];
        const char nextC = (i >= (sContent.size() - 1)) ? -1 : sContent[i + 1];
        if (c == '\n') {
            ++result->currentLine;
        }

        bool ret = result->procNextChar(c, nextC);
        if (!ret) {
            return nullptr;
        }
        i += lx->charactersToEat;
        lx->charactersToEat = 0;

        ++count;
    }

    bool ret = result->procEnd();
    ctx->popOneLevel();
    return ret ? result : nullptr;
}

void SLex::validateName(const std::string& name) {
}

void SLex::validate() const {
    for (auto item : items) {
        switch (item->itemType) {
            case SLexItem::Type::Tag: {
                SLexTag* tag = static_cast<SLexTag*>(item.get());
                validateName(tag->tagName);
            } break;
            case SLexItem::Type::Identifier: {
                SLexIdentifier* id = static_cast<SLexIdentifier*>(item.get());
                validateName(id->idName);
            } break;
            case SLexItem::Type::Equal:
                break;
            default:
                assert(false);
        }
    }
}

void SLex::_dump() const {
    printf("dump lexer, there are %d tokens\n", (int)items.size());
    for (int i = 0; i < int(items.size()); ++i) {
        // for (auto item : items) {
        auto item = items[i];
        printf("tok[%d] #%d ", i, item->lineNumber);
        switch (item->itemType) {
            case SLexItem::Type::Tag: {
                SLexTag* tag = static_cast<SLexTag*>(item.get());
                printf("tag=%s\n", tag->tagName.c_str());
            } break;
            case SLexItem::Type::Identifier: {
                SLexIdentifier* id = static_cast<SLexIdentifier*>(item.get());
                printf("id=%s\n", id->idName.c_str());
            } break;
            case SLexItem::Type::Equal:
                printf("Equal\n");
                break;
            default:
                assert(false);
        }
    }
    fflush(stdout);
}

bool SLex::procNextChar(char c, char nextC) {
    switch (state) {
        case State::Ready:
            return procFreshChar(c, nextC);
        case State::InTag:
            return procNextTagChar(c);
        case State::InComment:
            return procNextCommentChar(c, nextC);
        case State::InInclude:
            return procNextIncludeChar(c);
        case State::InIdentifier:
            return procNextIdentifierChar(c);
        case State::InDefine:
            return procStateNextDefineChar(c);
        case State::InHash:
            return procStateNextHashChar(c);
        default:
            assert(false);
    }
    assert(false);
    return true;
}

bool SLex::error(const std::string& err) {
    SqStream st;
    st.add(err);
    st.add(" at line ");
    st.add(currentLine + 1);
    context->logError(st.str());
    return false;
}

bool SLex::procStateNextHashChar(char c) {
    switch (c) {
        case 'i':
            state = State::InInclude;
            includeSubState = IncludeSubState::MatchingOpcode;
            curItem = "i";
            return true;
        case 'd':
            state = State::InDefine;
            defineSubState = DefineSubState::MatchingOpcode;
            curItem = "d";
            return true;
        default:
            // TODO: tests case #xx
            assert(false);
            return false;
    }
}

bool SLex::procStateNextDefineChar(char c) {
    static std::string defineStr("define");
    switch (defineSubState) {
        case DefineSubState::MatchingOpcode:
            curItem += c;
            if (defineStr.find(curItem) != 0) {
                return error("Malformed #define");
            }
            if (curItem == defineStr) {
                defineSubState = DefineSubState::MatchingSpace;
                curItem.clear();
                spaceCount = 0;
            }
            return true;

        case DefineSubState::MatchingSpace:
            if (isspace(c)) {
                spaceCount++;
                return true;
            }
            if (spaceCount > 0) {
                curItem.clear();
                curItem += c;
                defineSubState = DefineSubState::MatchingLhs;
                return true;
            }

            assert(false);
            return false;

        case DefineSubState::MatchingSpace2:
            if (isspace(c)) {
                spaceCount++;
                return true;
            }
            if (spaceCount > 0) {
                defineSubState = DefineSubState::MatchingRhs;
                curItem.clear();
                curItem += c;
                // need to save off char we just saw (in the future, if we care about the content
                return true;
            }

            assert(false);
            return false;

        case DefineSubState::MatchingLhs:
            if (isspace(c)) {
                defineVarName = curItem;
                defineSubState = DefineSubState::MatchingSpace2;
                spaceCount = 1;
                return true;
            } else {
                curItem += c;
            }
            return true;
        case DefineSubState::MatchingRhs:
            if (isspace(c)) {
                // when we finish rhs, we are done
                defineValue = curItem;
                curItem.clear();

                context->addDefine(defineVarName, defineValue);
                // 3 continue lexing
                state = State::Ready;
                return true;
            } else {
                curItem += c;
            }
            return true;

        default:
            assert(false);
    }
    return true;
}

bool SLex::procNextIncludeChar(char c) {
    static std::string includeStr("include");
    switch (includeSubState) {
        case IncludeSubState::MatchingOpcode:
            curItem += c;
            if (includeStr.find(curItem) != 0) {
                //SQINFO("bad item: >%s<", curItem.c_str());
                return error("Malformed #include");
            }
            if (curItem == includeStr) {
                includeSubState = IncludeSubState::MatchingSpace;
                spaceCount = 0;
            }
            return true;

        case IncludeSubState::MatchingSpace:
            if (isspace(c)) {
                spaceCount++;
                return true;
            }
            if (spaceCount > 0) {
                includeSubState = IncludeSubState::MatchingFileName;
                curItem = c;
                assert(curItem.size() == 1);
                return true;
            }
            assert(false);
            return false;

        case IncludeSubState::MatchingFileName:
            if (c == '\n') {
                assert(false);
                return false;
            }
            curItem += c;
            if ((c == '"') && curItem.size() > 1) {
                // OK, here we found a file name!
                return handleIncludeFile(curItem);
            }
            return true;

        default:
            assert(false);
    }

    // for now just keep eating chars
    return true;
}

bool SLex::procNextCommentChar(char c, char nextC) {
    if (commentSubState == CommentSubState::MatchingRegularComment) {
        if (c == 10 || c == 13) {
            //inComment = false;
            state = State::Ready;
        }
    } else {
        if ((c == '*') && (nextC == '/')) {
            state = State::Ready;
            charactersToEat++;
        }
    }
    return true;
}

bool SLex::procFreshChar(char c, char nextC) {
    if (isspace(c)) {
        return true;  // eat whitespace
    }
    switch (c) {
        case '<':
            state = State::InTag;
            return true;
        case '/':
            if (nextC == '/' || nextC == '*') {
                state = State::InComment;
                commentSubState = (nextC == '*') ? CommentSubState::MatchingMultilineComment : CommentSubState::MatchingRegularComment;
                return true;
            }
            break;
        case '=':
            addCompletedItem(std::make_shared<SLexEqual>(currentLine), false);
            return true;
        case '#':
            state = State::InHash;
            return true;
    }

    state = State::InIdentifier;
    curItem.clear();
    curItem += c;
    validateName(curItem);
    return true;
}

bool SLex::procNextTagChar(char c) {
    if (isspace(c)) {
        return false;  // can't have white space in the middle of a tag
    }
    if (c == '<') {
        return false;
    }
    if (c == '>') {
        validateName(curItem);
        addCompletedItem(std::make_shared<SLexTag>(curItem, currentLine), true);
        //inTag = false;
        state = State::Ready;
        return true;
    }

    curItem += c;  // do we care about line feeds?
    validateName(curItem);
    return true;
}

bool SLex::procEnd() {
    if (state == State::InIdentifier) {
        validateName(curItem);
        addCompletedItem(std::make_shared<SLexIdentifier>(curItem, currentLine), true);
        return true;
    }

    if (state == State::InTag) {
        return false;
    }

    return true;
}

bool SLex::procNextIdentifierChar(char c) {
    if (c == '=') {
        return procEqualsSignInIdentifier();
    }

    // check for a comment terminating a string
    if (c == '/') {
        if (lastCharWasForwardSlash) {
            if (!curItem.empty()) {
                assert(curItem.back() == '/');
                curItem.pop_back();
            }
            // remove trailing space
            while (!curItem.empty() && isspace(curItem.back())) {
                curItem.pop_back();
            }

            addCompletedItem(std::make_shared<SLexIdentifier>(curItem, currentLine), true);
            state = State::InComment;
            return true;
        }
        lastCharWasForwardSlash = true;
    } else {
        lastCharWasForwardSlash = false;
    }
    // terminate identifier on these, but proc them
    // TODO, should the middle one be '>'? is that just an error?
    if (c == '<' || c == '<' || c == '=' || c == '\n') {
        addCompletedItem(std::make_shared<SLexIdentifier>(curItem, currentLine), true);
        //inIdentifier = false;
        state = State::Ready;
        return procFreshChar(c);
    }

    // We only terminate on a space if we are not parsing a String type opcode
    const bool terminatingSpace = isspace(c) && !lastIdentifierIsString;
    // terminate on these, but don't proc
    if (terminatingSpace) {
        addCompletedItem(std::make_shared<SLexIdentifier>(curItem, currentLine), true);
        state = State::Ready;
        return true;
    }

    assert(state == State::InIdentifier);
    curItem += c;
    validateName(curItem);
    return true;
}

bool SLex::procEqualsSignInIdentifier() {
    if (lastIdentifierIsString) {
        // If we get an equals sign in the middle of a sample file name (or other string), then we need to adjust.
        // for things other than sample we don't accept spaces, so there is no issue.

        // The last space is going to the the character right before the next identifier.
        auto lastSpacePos = curItem.rfind(' ');
        if (lastSpacePos == std::string::npos) {
            //SQWARN("equals sign found in identifier at line %d", currentLine);
            return false;  // error
        }
        // todo: multiple spaces
        // std::string fileName = curItem.substr(0, lastSpacePos);

        std::string nextId = curItem.substr(lastSpacePos + 1);
        auto filenameEndIndex = lastSpacePos;
        int searchIndex = int(lastSpacePos);
        while (searchIndex >= 0 && curItem.at(searchIndex) == ' ') {
            filenameEndIndex = searchIndex;
            searchIndex--;
        }
        std::string fileName = curItem.substr(0, filenameEndIndex);

        addCompletedItem(std::make_shared<SLexIdentifier>(fileName, currentLine), true);
        addCompletedItem(std::make_shared<SLexIdentifier>(nextId, currentLine), true);
        //inIdentifier = false;
        state = State::Ready;
        return procFreshChar('=');
    } else {
        // if it's not a sample file, then process normally. Just finish identifier
        // and go on with the equals sign/
        addCompletedItem(std::make_shared<SLexIdentifier>(curItem, currentLine), true);
        // inIdentifier = false;
        state = State::Ready;
        return procFreshChar('=');
    }
}

void SLex::addCompletedItem(SLexItemPtr item, bool clearCurItem) {
    items.push_back(item);
    if (clearCurItem) {
        curItem.clear();
    }
    if (item->itemType == SLexItem::Type::Identifier) {
        SLexIdentifier* ident = static_cast<SLexIdentifier*>(item.get());

        // check if we are an opcode that takes a string value, like path=foo bar.wav
        // They get treated special
        lastIdentifierIsString = SamplerSchema::isFreeTextType(ident->idName);

        context->applyDefine(&ident->idName);
    }
}

std::string SLexItem::lineNumberAsString() const {
    char buf[100];
    snprintf(buf, sizeof(buf), "%d", lineNumber);
    return buf;
}

bool SLex::handleIncludeFile(const std::string& relativeFileName) {
    //SQINFO("SLex::handleIncludeFile %s", fileName.c_str());
    assert(!relativeFileName.empty());

    bool bOK = context->pushOneLevel(relativeFileName, currentLine);
    if (!bOK) {
        return false;
    }

    // now need to recurse
    auto includeLexer = SLex::goRecurse(context);
    if (!includeLexer) {
        return false;  // error should already be in outErrorStringPtr
    }
    //assert(context->includeRecursionDepth == before + 1);
    // 2) copy the tokens from include to this.
    this->items.insert(
        this->items.end(),
        std::make_move_iterator(includeLexer->items.begin()),
        std::make_move_iterator(includeLexer->items.end()));
    //SQINFO("finished incl, curItem=%s", curItem.c_str());
    curItem.clear();
    // 3 continue lexing
    state = State::Ready;
    return true;
}
