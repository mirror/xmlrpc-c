/**
   This json parser are written in c to be ar part of the xmlrpc
   library, and using the xmlrpc-c memory handling and type system.

   Json : rfc-4627
*/
#include "xmlrpc_config.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "xmlrpc-c/json.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"



/*=============================================================================
    Tokenizer for the json parser
=============================================================================*/
enum ttype {
    typeNone,
    typeOpenBrace,
    typeCloseBrace,
    typeOpenBracket,
    typeCloseBracket,
    typeColon,
    typeComma,
    typeString,
    typeInteger,
    typeFloat,
    typeNull,
    typeTrue,
    typeFalse,
    typeEof,
} ;

static const char *
tokTypeName(enum ttype const type) {

    switch (type) {
    case typeNone:         return "None";
    case typeOpenBrace:    return "Open brace";
    case typeCloseBrace:   return "Close brace";
    case typeOpenBracket:  return "Open bracket";
    case typeCloseBracket: return "Close bracket";
    case typeColon:        return "Colon";
    case typeComma:        return "Comma";
    case typeString:       return "String";
    case typeInteger:      return "Integer";
    case typeFloat:        return "Float";
    case typeNull:         return "Null";
    case typeTrue:         return "True";
    case typeFalse:        return "False";
    case typeEof:          return "Eof";
    default:               return "???";
    }
}



typedef struct {
    const char * original;
    size_t       size;
    const char * begin;
    const char * end;
    enum ttype   type;
} Tokenizer;



static void
initializeTokenizer(Tokenizer *  const tokP,
                    const char * const str) {

    tokP->original = str;
    tokP->end      = str;  /* end of the "previous" token */
    tokP->type     = typeNone;
}



static void
terminateTokenizer(Tokenizer * const tokP ATTR_UNUSED ) {

}



struct docPosition {
    /* A position in the document, as meaningful to the user */
    unsigned int lineNum;  /* First line is 1 */
    unsigned int colNum;   /* First column is 1 */
};



static struct docPosition
currentDocumentPosition(Tokenizer * const tokP) {
/*----------------------------------------------------------------------------
   Return the document position (line & column) of the start of the current
   token
-----------------------------------------------------------------------------*/
    struct docPosition retval;

    unsigned int curLine;
    unsigned int curCol;
    const char * cursor;

    curLine = 0;
    curCol  = 0;

    for (cursor = tokP->original; cursor < tokP->begin; ++cursor) {
        ++curCol;

        if (*cursor == '\n') {
            ++curLine;
            curCol = 0;
        }
    }
    retval.lineNum = curLine + 1;
    retval.colNum  = curCol  + 1;

    return retval;
}



static void
setParseErr(xmlrpc_env *   const envP,
            Tokenizer * const tokP,
            const char *   const format,
            ...) {

    struct docPosition const pos = currentDocumentPosition(tokP);

    va_list args;
    const char * msg;

    XMLRPC_ASSERT(envP != NULL);
    XMLRPC_ASSERT(format != NULL);

    va_start(args, format);

    xmlrpc_vasprintf(&msg, format, args);

    xmlrpc_env_set_fault_formatted(
        envP, XMLRPC_PARSE_ERROR,
        "JSON parse error at Line %u, Column %u: %s",
        pos.lineNum, pos.colNum, msg);

    xmlrpc_strfree(msg);

    va_end(args);
}



static void
finishStringToken(xmlrpc_env *   const envP,
                  Tokenizer * const tokP) {

    ++tokP->end;

    while (*tokP->end != '"' && *tokP->end != '\0' && !envP->fault_occurred) {
        if (*tokP->end == '\\') {
            ++tokP->end;
            switch (*tokP->end) {
            case '"':
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
                tokP->end++;
                break;
            case 'u': {
                const char *cur = ++tokP->end;
                    
                while( isxdigit(*cur))
                    ++cur;
                
                if (cur - tokP->end != 4)
                    setParseErr(envP, tokP,
                                "hex unicode must contain 4 digits.  "
                                "There are %u here", cur - tokP->end);
                else
                    tokP->end = cur;
            } break;

            default:
                setParseErr(envP, tokP, "unknown escape character "
                            "after backslash: '%c'", *tokP->end);
            }
        } else
            ++tokP->end;
    }
    if (!envP->fault_occurred) {
        if (*tokP->end == '\0')
            setParseErr(envP, tokP, "JSON document ends in the middle "
                        "of a backslash escape sequence");
        else {
            ++tokP->end;
            tokP->size = (tokP->end - tokP->begin) - 1;
        }
    }
}



static bool
isInteger(const char * const token,
          unsigned int const tokSize) {

    if (tokSize < 1)
        return false;
    else {
        unsigned int i;

        i = 0;

        if (token[0] == '-')
            ++i;

        while (i < tokSize) {
            if (!isdigit(token[i]))
                return false;
            ++i;
        }
        return true;
    }
}



static bool
isFloat(const char * const token,
        unsigned int const tokSize) {

    unsigned int i;
    bool seenPeriod;
    bool seenDigit;

    seenPeriod = false;
    seenDigit  = false;
    i = 0;

    if (tokSize >= 1 && token[0] == '-')
        ++i;

    while (i < tokSize) {
        char const c = token[i];

        if (c == 'e')
            return isInteger(&token[i], tokSize - i);
        else if (c == '.') {
            if (seenPeriod) {
                /* It's a second period */
                return false;
            } else {
                seenPeriod = true;
            }
        } else if (isdigit(c))
            seenDigit = true;
        else
            return false;
        ++i;
    }
    if (seenDigit)
        return true;
    else
        return false;
}



static bool
isWordChar(char const candidate) {
/*----------------------------------------------------------------------------
   Return true iff 'candidate' is a character that can be in a "word" token.
   A word token is a multi-character token that is either a JSON keyword or a
   number.
-----------------------------------------------------------------------------*/
    return (isalnum(candidate) || candidate == '.' || candidate == '-');
}



static void
finishAlphanumericWordToken(Tokenizer * const tokP) {

    ++tokP->end;

    while (isWordChar(*tokP->end))
        ++tokP->end;

    tokP->size = tokP->end - tokP->begin;
}



static void
finishDelimiterToken(Tokenizer * const tokP) {

    ++tokP->end;
    tokP->size = tokP->end - tokP->begin;
}



static bool
atComment(Tokenizer * const tokP) {

    return (*tokP->begin == '/' && *(tokP->begin + 1) == '/');
}



static void
advancePastWhiteSpace(Tokenizer * const tokP) {

    while (isspace(*tokP->begin))
        ++tokP->begin;
}



static void
advancePastComments(Tokenizer * const tokP) {
/*----------------------------------------------------------------------------
   Advance the pointer over any comments.
-----------------------------------------------------------------------------*/
    while (atComment(tokP)) {
        /* A comment ends at a newline or end of document */
        while (*tokP->begin != '\n' && *tokP->begin != '\0')
            ++tokP->begin;
    }
}



static void
advanceToNextToken(Tokenizer * const tokP) {
/*----------------------------------------------------------------------------
   Advance the pointer over any white space and comments to the next
   token, or end of document, whichever comes first.
-----------------------------------------------------------------------------*/

    while (*tokP->begin != '\0' &&
           (isspace(*tokP->begin) || atComment(tokP))) {
        
        advancePastWhiteSpace(tokP);

        advancePastComments(tokP);
    }
}



static void
getToken(xmlrpc_env *   const envP,
         Tokenizer * const tokP) {

    /* The token starts where the last one left off */
    tokP->begin = tokP->end;

    advanceToNextToken(tokP);

    if (*tokP->begin == '\0') {
        /* End of document */
        tokP->end = tokP->begin;
        tokP->type = typeEof;
        tokP->size = tokP->end - tokP->begin;
    } else {
        tokP->end = tokP->begin;  /* initial value */
    
        if (*tokP->begin == '{') {
            finishDelimiterToken(tokP);
            tokP->type = typeOpenBrace;
        } else if (*tokP->begin == '}') {
            finishDelimiterToken(tokP);
            tokP->type = typeCloseBrace;
        } else if (*tokP->begin == '[') {
            finishDelimiterToken(tokP);
            tokP->type = typeOpenBracket;
        } else if (*tokP->begin == ']') {
            finishDelimiterToken(tokP);
            tokP->type = typeCloseBracket;
        } else if (*tokP->begin == ':') {
            finishDelimiterToken(tokP);
            tokP->type = typeColon;
        } else if (*tokP->begin == ',') {
            finishDelimiterToken(tokP);
            tokP->type = typeComma;
        } else if (*tokP->begin == '"') {
            finishStringToken(envP, tokP);

            if (!envP->fault_occurred)
                tokP->type = typeString;
        } else {
            if (isWordChar(*tokP->begin)) {
                finishAlphanumericWordToken(tokP);

                if (isInteger(tokP->begin, tokP->size))
                    tokP->type = typeInteger;
                else if (isFloat(tokP->begin, tokP->size))
                    tokP->type = typeFloat;
                else if (strncmp(tokP->begin,  "null", tokP->size ) == 0)
                    tokP->type = typeNull;
                else if(strncmp(tokP->begin, "false", tokP->size) == 0)
                    tokP->type = typeFalse;
                else if(strncmp(tokP->begin, "true", tokP->size) == 0)
                    tokP->type = typeTrue;
                else
                    setParseErr(envP, tokP, "Invalid word token -- "
                                "Not a valid integer, floating point "
                                "number, 'null', 'true', or 'false'");
            } else {
                setParseErr(envP, tokP,
                            "Not a valid token -- starts with '%c'; "
                            "a valid token starts with "
                            "one of []{}:,\"-. or digit or letter",
                            *tokP->begin);
            }
        }
    }
}



/*===========================================================================*/



static int
utf8Decode(uint32_t const c,
           char *   const out) {
/*---------------------------------------------------------------------------
  convert a unicode char to a utf8 char
---------------------------------------------------------------------------*/
    if (c <= 0x7F) { /* 0XXX XXXX one byte */
        out[0] = (char) c;
        return  1;
    } else if (c <= 0x7FF) { /* 110X XXXX  two bytes */
        out[0] = (char)( 0xC0 | (c >> 6) );
        out[1] = (char)( 0x80 | (c & 0x3F) );
        return 2;
    } else if (c <= 0xFFFF) { /* 1110 XXXX  three bytes */
        out[0] = (char) (0xE0 | (c >> 12));
        out[1] = (char) (0x80 | ((c >> 6) & 0x3F));
        out[2] = (char) (0x80 | (c & 0x3F));
        return 3;
     } else if (c <= 0x1FFFFF) { /* 1111 0XXX  four bytes */
        out[0] = (char) (0xF0 | (c >> 18));
        out[1] = (char) (0x80 | ((c >> 12) & 0x3F));
        out[2] = (char) (0x80 | ((c >> 6) & 0x3F));
        out[3] = (char) (0x80 | (c & 0x3F));
        return 4;
     } else
        return 0;
}



static void
getBackslashSequence(xmlrpc_env *       const envP,
                     const char *       const cur,
                     xmlrpc_mem_block * const memBlockP,
                     unsigned int *     const nBytesConsumedP) {

    char buffer[5];
    unsigned int tsize;

    switch (*cur) {
    case '"':
        buffer[0] = '"';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case '/':
        buffer[0] = '/';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case '\\':
        buffer[0] = '\\';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case 'b':
        buffer[0] = '\b';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case 'f':
        buffer[0] = '\f';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case 'n':
        buffer[0] = '\n';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case 'r':
        buffer[0] = '\r';
        tsize = 1;
        *nBytesConsumedP = 1;
        break;
    case 'u': {
        long digit;
        strncpy(buffer, cur + 1, 4);
        digit = strtol(buffer, NULL, 16);
        tsize = utf8Decode(digit, buffer);
        *nBytesConsumedP = 4;
        break;
    }
    default:
        xmlrpc_faultf(envP, "Invalid character after backslash "
                      "escape: '%c'", *cur);
        tsize = 0; /* quiet compiler warning */
    }
    if (!envP->fault_occurred)
        XMLRPC_MEMBLOCK_APPEND(char, envP, memBlockP, buffer, tsize );
}



static void
unescapeString(xmlrpc_env *       const envP,
               const char *       const begin,
               const char *       const end,
               xmlrpc_mem_block * const memBlockP) {

    XMLRPC_MEMBLOCK_INIT(char, envP, memBlockP, 0);

    if (!envP->fault_occurred) {
        const char * cur;
        const char * last;

        cur = begin;
        last = cur;
            
        while (cur != end && !envP->fault_occurred) {
            if (*cur == '\\') {
                unsigned int nBytesConsumed;
                if (cur != last) {
                    XMLRPC_MEMBLOCK_APPEND(
                        char, envP, memBlockP, last, cur - last );
                    if (!envP->fault_occurred)
                        last = cur;
                }
                if (!envP->fault_occurred) {
                    cur += 1;

                    getBackslashSequence(envP, cur, memBlockP,
                                         &nBytesConsumed);

                    if (!envP->fault_occurred) {
                        cur += nBytesConsumed;
                        last = cur;
                    }
                }
            } else
                ++cur;
        }
        if (!envP->fault_occurred) {
            if (cur != last) {
                XMLRPC_MEMBLOCK_APPEND(char, envP,
                                       memBlockP, last, cur - last );
            }
        }
        if (!envP->fault_occurred) {
            /* Append terminating NUL */
            XMLRPC_MEMBLOCK_APPEND(char, envP, memBlockP, "", 1);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_CLEAN(char, memBlockP);
    }
}



static xmlrpc_value *
makeUtf8String(xmlrpc_env * const envP,
               const char * const begin,
               const char * const end) {
/*---------------------------------------------------------------------------- 
    Copy a json string directly into a string value, and convert any json
    escaping (\uXXXX) to something acceptable to the internal string handling.

    Try to do this in as few chunks as possible !
-----------------------------------------------------------------------------*/
    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_STRING;
        valP->_wcs_block = NULL;

        if (!envP->fault_occurred)
            unescapeString(envP, begin, end, &valP->_block);

        if (envP->fault_occurred)
            xmlrpc_DECREF(valP);
    }
    return valP;
}



static xmlrpc_value *
stringTokenValue(xmlrpc_env *   const envP,
                 Tokenizer * const tokP) {

    xmlrpc_env env;
    xmlrpc_value * valP;

    xmlrpc_env_init(&env);

    assert(tokP->end >= tokP->begin + 2);
    assert(*tokP->begin == '"');
    assert(*(tokP->end-1) == '"');
    
    valP = makeUtf8String(&env, tokP->begin + 1, tokP->end - 1);

    if (env.fault_occurred) {
        setParseErr(envP, tokP, "Error in string token: %s",
                    env.fault_string);
    }
    xmlrpc_env_clean(&env);

    return valP;
}


/* Forward declarations for recursion: */

static xmlrpc_value *
parseValue(xmlrpc_env *    const envP,
           Tokenizer *  const tokP);

static xmlrpc_value *
parseList(xmlrpc_env *   const envP,
          Tokenizer * const tokP);

static xmlrpc_value *
parseObject(xmlrpc_env *   const envP,
            Tokenizer * const tokP);



static void
parseListElement(xmlrpc_env *   const envP,
                 Tokenizer * const tokP,
                 xmlrpc_value * const listArrayP,
                 bool *         const endOfListP) {

    xmlrpc_value * itemP;

    itemP = parseValue(envP, tokP);

    if (!envP->fault_occurred) {
        xmlrpc_array_append_item(envP, listArrayP, itemP);

        if (!envP->fault_occurred) {
            getToken(envP, tokP);
            if (!envP->fault_occurred) {
                if (tokP->type == typeComma) {
                    *endOfListP = false;
                } else if (tokP->type == typeCloseBracket)
                    *endOfListP = true;
                else
                    setParseErr(envP, tokP,
                                "Need comma or close bracket "
                                "after array item.  Instead we have %s",
                                tokTypeName(tokP->type));
            }
        }
        xmlrpc_DECREF(itemP);
    }
}



static xmlrpc_value *
parseList(xmlrpc_env *   const envP,
          Tokenizer * const tokP) {

    xmlrpc_value * retval;

    XMLRPC_ASSERT_ENV_OK(envP);

    retval = xmlrpc_array_new(envP);
    
    if (!envP->fault_occurred) {
        bool endOfList;
        for (endOfList = false; !endOfList && !envP->fault_occurred; ) {
            getToken(envP,tokP);

            if (!envP->fault_occurred) {
                if (tokP->type == typeEof)
                    endOfList = true;
                else if (tokP->type == typeCloseBracket)
                    endOfList = true;
                else
                    parseListElement(envP, tokP, retval, &endOfList);
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(retval);
    }
    return retval;
}



static void
parseObjectMemberValue(xmlrpc_env *   const envP,
                       Tokenizer * const tokP,
                       xmlrpc_value * const keyP,
                       xmlrpc_value * const objectP) {

    xmlrpc_value * valP;

    getToken(envP,tokP);

    if (!envP->fault_occurred) {
        valP = parseValue(envP, tokP);

        if (!envP->fault_occurred) {
            xmlrpc_struct_set_value_v(envP, objectP, keyP, valP);

            xmlrpc_DECREF(valP);
        }
    }
}



static void
parseObjectMember(xmlrpc_env *   const envP,
                  Tokenizer * const tokP,
                  xmlrpc_value * const objectP) {

    xmlrpc_env env;
    xmlrpc_value * keyP;

    xmlrpc_env_init(&env);

    /* The current token is the string which is the member name: */
    assert(tokP->type = typeString);
    assert(tokP->end >= tokP->begin + 2);
    assert(*tokP->begin == '"');
    assert(*(tokP->end-1) == '"');
                      
    keyP = makeUtf8String(&env, tokP->begin + 1, tokP->end - 1);
                
    if (env.fault_occurred)
        setParseErr(envP, tokP, "Error in what is supposed to be "
                    "the key of a member of an object: %s",
                    env.fault_string);
    else {
        getToken(envP, tokP);

        if (!envP->fault_occurred) {
            if (tokP->type == typeColon)
                parseObjectMemberValue(envP, tokP, keyP, objectP);
            else
                setParseErr(envP, tokP,
                            "Need a colon after member key "
                            "in object.  Instead we have %s",
                            tokTypeName(tokP->type));
        }
        xmlrpc_DECREF(keyP);
    }
    xmlrpc_env_clean(&env);
}


 
static xmlrpc_value *
parseObject(xmlrpc_env *   const envP,
            Tokenizer * const tokP) {

    xmlrpc_value * retval;

    XMLRPC_ASSERT_ENV_OK(envP);

    retval = xmlrpc_struct_new(envP);

    if (!envP->fault_occurred) {
        bool objectDone;

        objectDone = false;
        while (!objectDone && !envP->fault_occurred) {
            getToken(envP, tokP);

            if (!envP->fault_occurred) {
                if (tokP->type == typeCloseBrace) {
                    objectDone = true;
                } else if (tokP->type == typeString) {
                    parseObjectMember(envP, tokP, retval);

                    if (!envP->fault_occurred) {
                        getToken(envP, tokP);
                        
                        if (!envP->fault_occurred) {
                            if (tokP->type == typeComma) {
                                /* member separator; keep going */
                            } else if (tokP->type == typeCloseBrace) {
                                /* No more members in this object */
                                objectDone = true;
                            } else
                                setParseErr(
                                    envP, tokP,
                                    "Need a comma or close brace after object "
                                    "member.  Instead we have %s",
                                    tokTypeName(tokP->type));
                        }
                    }
                } else {
                    setParseErr(envP, tokP,
                                "Need a string (i.e. starting with "
                                "a quotation mark) as member key "
                                "in object, or closing brace to end the "
                                "object.  Instead we have %s",
                                tokTypeName(tokP->type));
                }
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(retval);
    }
    return retval;
}



static xmlrpc_value *
parseValue(xmlrpc_env *   const envP,
           Tokenizer * const tokP) {

    xmlrpc_value * retval;
    
    XMLRPC_ASSERT_ENV_OK(envP);
        
    switch (tokP->type) {

    case typeOpenBracket:
        retval = parseList(envP, tokP);
        break;

    case typeOpenBrace:
        retval = parseObject(envP, tokP);
        break;
            
    case typeNull:
        retval = xmlrpc_nil_new(envP);
        break;

    case typeFalse:
        retval = xmlrpc_bool_new(envP, (xmlrpc_bool)false);
        break;

    case typeTrue:
        retval = xmlrpc_bool_new(envP, (xmlrpc_bool)true);
        break;

    case typeInteger:
        retval = xmlrpc_int_new(envP, atoi(tokP->begin));
        break;
        
    case typeFloat:
        retval = xmlrpc_double_new(envP, strtod(tokP->begin, NULL));
        break;

    case typeString:
        retval = stringTokenValue(envP, tokP);
        break;
            
    default:
        retval = NULL;
        setParseErr(envP, tokP, "Invalid token "
                    "where a value is supposed to begin: %s.  "
                    "Should be an open bracket, open brace, "
                    "'null', 'false', 'true', a number, or a string",
                    tokTypeName(tokP->type));
    }
    return retval;
}



xmlrpc_value *
xmlrpc_parse_json(xmlrpc_env * const envP,
                  const char * const str) {

    xmlrpc_value * retval;
    Tokenizer tok;
    
    XMLRPC_ASSERT_ENV_OK(envP);
    
    initializeTokenizer(&tok, str);

    getToken(envP, &tok);

    if (!envP->fault_occurred) {
        retval = parseValue(envP, &tok);

        if (!envP->fault_occurred) {
            getToken(envP, &tok);

            if (!envP->fault_occurred) {
                if (tok.type != typeEof)
                    setParseErr(envP, &tok, "There is junk after the end of "
                                "the JSON value, to wit a %s token",
                                tokTypeName(tok.type));
            }
            if (envP->fault_occurred)
                xmlrpc_DECREF(retval);
        }
    } else 
        retval = NULL;  /* quiet compiler warning */

    terminateTokenizer(&tok);

    return retval;
}



/*============================================================================
      Serialize value to JSON
============================================================================*/

/* Borrowed from xmlrpc_serialize */

static void
formatOut(xmlrpc_env *       const envP,
          xmlrpc_mem_block * const outputP,
          const char *       const formatString, ... ) {

    va_list args;
    char buffer[1024];
    int rc;

    XMLRPC_ASSERT_ENV_OK(envP);

    va_start(args, formatString);

    rc = XMLRPC_VSNPRINTF(buffer, sizeof(buffer), formatString, args);

    /* Old vsnprintf() (and Windows) fails with return value -1 if the full
       string doesn't fit in the buffer.  New vsnprintf() puts whatever will
       fit in the buffer, and returns the length of the full string
       regardless.  For us, this truncation is a failure.
    */

    if (rc < 0)
        xmlrpc_faultf(envP, "formatOut() overflowed internal buffer");
    else {
        unsigned int const formattedLen = rc;

        if (formattedLen + 1 >= (sizeof(buffer)))
            xmlrpc_faultf(envP, "formatOut() overflowed internal buffer");
        else
            XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, buffer, formattedLen);
    }
    va_end(args);
}



static void
indent(xmlrpc_env *       const envP,
       unsigned int       const level,
       xmlrpc_mem_block * const outP) {

    unsigned int i;

    for (i = 0; i < level * 2 && !envP->fault_occurred; ++i)
        XMLRPC_MEMBLOCK_APPEND(char, envP, outP, " ", 1);
}



/* Forward declaration for recursion */

static void
serializeValue(xmlrpc_env *       const envP,
               xmlrpc_value *     const valP,
               unsigned int       const level,
               xmlrpc_mem_block * const outP);



static void
makeJsonString(xmlrpc_env *       const envP,
               const char *       const value,
               size_t             const length,
               xmlrpc_mem_block * const outP) {
/*----------------------------------------------------------------------------
  Create a JSON representation of a string, appended to *outP.
-----------------------------------------------------------------------------*/
    const char * const begin = &value[0];
    const char * const end   = begin + length;

    const char * cur;
    const char * last;

    last = cur = begin;
    
    while (cur != end && !envP->fault_occurred) {
        if (strchr("\"/\\\b\f\n\r", *cur) ||
            ((unsigned char)*cur) < 0x1F) {

            unsigned int size;
            char buffer[8];

            switch (*cur) {
            case '\"':
                buffer[1] = '"';
                size = 2;
                break;
            case '/':
                buffer[1] = '/';
                size = 2;
                break;
            case '\\':
                buffer[1] = '\\';
                size = 2;
                break;
            case '\b':
                buffer[1] = 'b';
                size = 2;
                break;
            case '\f':
                buffer[1] = 'f';
                size = 2;
                break;
            case '\n':
                buffer[1] = 'n';
                size = 2;
                break;
            case '\r':
                buffer[1] = 'r';
                size = 2;
                break;
            default: {
                    unsigned char const c = (unsigned char)*cur;

                    assert(c < 0x1F);
                    size = sprintf(buffer, "\\u%04x", c);
                }
            }
            if (cur != last) {
                XMLRPC_MEMBLOCK_APPEND(char, envP, outP, last, cur - last);
                if (!envP->fault_occurred)
                    last = cur;
            }
            if (!envP->fault_occurred){
                buffer[0] = '\\';
                XMLRPC_MEMBLOCK_APPEND(char, envP, outP, buffer, size);
            }
        }
        ++cur;
    }

    if (cur != last)
        XMLRPC_MEMBLOCK_APPEND(char, envP, outP, last, cur - last);

    if (envP->fault_occurred)
        XMLRPC_MEMBLOCK_CLEAN(char, outP);
}



static void
makeJsonStringFromXmlRpc(xmlrpc_env *         const envP,
                         const xmlrpc_value * const valP,
                         xmlrpc_mem_block *   const outP) {
/*----------------------------------------------------------------------------
  Convert a string XML-RPC value to JSON, appended to *outP.
-----------------------------------------------------------------------------*/
    const char * value;
    size_t length;

    xmlrpc_read_string_lp(envP, valP, &length, &value);
    if (!envP->fault_occurred) {
        makeJsonString(envP, value, length, outP);

        xmlrpc_strfree(value);
    }
}



static void
serializeString(xmlrpc_env * const envP,
                xmlrpc_value *     const valP,
                xmlrpc_mem_block * const outP) {
    
    formatOut(envP, outP, "\"");
    
    makeJsonStringFromXmlRpc(envP, valP, outP);
    
    formatOut(envP, outP, "\"");
}
    


static void
serializeDatetime(xmlrpc_env *       const envP,
                  xmlrpc_value *     const valP,
                  xmlrpc_mem_block * const outP) {

    /* ISO 8601 time string as JSON does not have a datetime type */

    formatOut(envP, outP, "\"%u%02u%02uT%02u:%02u:%02u\"",
              valP->_value.dt.Y,
              valP->_value.dt.M,
              valP->_value.dt.D,
              valP->_value.dt.h,
              valP->_value.dt.m,
              valP->_value.dt.s);
}


        


static void
serializeBitstring(xmlrpc_env *       const envP,
                   xmlrpc_value *     const valP,
                   xmlrpc_mem_block * const outP) {

    /* Bit string as text whose ASCII coding is the the bit string,
       since JSON does not have a bit string type

       TODO: This should be in hexadecimal or base64 instead
    */
    
    unsigned char * const contents =
        XMLRPC_MEMBLOCK_CONTENTS(unsigned char, &valP->_block);
    size_t const size =
        XMLRPC_MEMBLOCK_SIZE(unsigned char, &valP->_block);
            
    if (!envP->fault_occurred) {
        formatOut(envP, outP, "\"");
            
        XMLRPC_MEMBLOCK_APPEND(char, envP, outP, contents, size);
        
        formatOut(envP, outP, "\"");
    }
}



static void
serializeArray(xmlrpc_env *       const envP,
               xmlrpc_value *     const valP,
               unsigned int       const level,
               xmlrpc_mem_block * const outP) {

    unsigned int const size = xmlrpc_array_size(envP, valP);

    if (!envP->fault_occurred) {
        unsigned int i;

        formatOut(envP, outP, "[\n");

        for (i = 0; i < size && !envP->fault_occurred; ++i) {
            xmlrpc_value * const itemP =
                xmlrpc_array_get_item(envP, valP, i);
                    
            if (!envP->fault_occurred) {
                if (!envP->fault_occurred) {
                    serializeValue(envP, itemP, level + 1, outP);

                    XMLRPC_MEMBLOCK_APPEND(char, envP, outP, ",\n", 2);
                }
            }
        }
        if (!envP->fault_occurred) {
            indent(envP, level, outP);
            if (!envP->fault_occurred) {
                XMLRPC_MEMBLOCK_APPEND(char, envP, outP, "]", 1);
            }
        }
    }
} 



static void
serializeStructMember(xmlrpc_env *       const envP,
                      xmlrpc_value *     const memberKeyP,
                      xmlrpc_value *     const memberValueP,
                      unsigned int       const level,
                      xmlrpc_mem_block * const outP) {

    serializeValue(envP, memberKeyP, level, outP);
                    
    if (!envP->fault_occurred) {
        formatOut(envP, outP, ":\n");
        
        if (!envP->fault_occurred) {
            serializeValue(envP, memberValueP, level + 1, outP);

            if (!envP->fault_occurred)
                XMLRPC_MEMBLOCK_APPEND(char, envP, outP, ",\n", 2);
        }
    }
}


 
static void
serializeStruct(xmlrpc_env *       const envP,
                xmlrpc_value *     const valP,
                unsigned int       const level,
                xmlrpc_mem_block * const outP) {

    indent(envP, level, outP);

    if (!envP->fault_occurred) {
        formatOut(envP, outP, "{\n");
        if (!envP->fault_occurred) {
            unsigned int const size = xmlrpc_struct_size(envP, valP);
            
            if (!envP->fault_occurred) {
                unsigned int i;
                for (i = 0; i < size && !envP->fault_occurred; ++i) {
                    xmlrpc_value * memberKeyP;
                    xmlrpc_value * memberValueP;

                    xmlrpc_struct_get_key_and_value(envP, valP, i,
                                                    &memberKeyP,
                                                    &memberValueP);
                    if (!envP->fault_occurred)
                        serializeStructMember(envP, memberKeyP, memberValueP,
                                              level + 1, outP);
                }
                if (!envP->fault_occurred) 
                    XMLRPC_MEMBLOCK_APPEND(char, envP, outP, "}", 1);
            }
        }
    }
}



static void
serializeValue(xmlrpc_env *       const envP,
               xmlrpc_value *     const valP,
               unsigned int       const level,
               xmlrpc_mem_block * const outP) {

    XMLRPC_ASSERT_ENV_OK(envP);

    indent(envP, level, outP);
        
    switch (valP->_type) {
    case XMLRPC_TYPE_INT:
        formatOut(envP, outP, "%d", valP->_value.i);
        break;

    case XMLRPC_TYPE_I8:
        formatOut(envP, outP, "%" PRId64, valP->_value.i8);
        break;

    case XMLRPC_TYPE_BOOL:
        formatOut(envP, outP, "%s", valP->_value.b ? "true" : "false");
        break;

    case XMLRPC_TYPE_DOUBLE:
        formatOut(envP, outP, "%e", valP->_value.d);
        break;

    case XMLRPC_TYPE_DATETIME:
        serializeDatetime(envP, valP, outP);
        break;

    case XMLRPC_TYPE_STRING:
        serializeString(envP, valP, outP);
        break;

    case XMLRPC_TYPE_BASE64:
        serializeBitstring(envP, valP, outP);
        break;      

    case XMLRPC_TYPE_ARRAY:
        serializeArray(envP, valP, level, outP);
        break;

    case XMLRPC_TYPE_STRUCT:
        serializeStruct(envP, valP, level, outP);
        break;

    case XMLRPC_TYPE_C_PTR:
        xmlrpc_faultf(envP, "Tried to serialize a C pointer value.");
        break;

    case XMLRPC_TYPE_NIL:
        formatOut(envP, outP, "null");
        break;

    case XMLRPC_TYPE_DEAD:
        xmlrpc_faultf(envP, "Tried to serialize a dead value.");
        break;

    default:
        xmlrpc_faultf(envP, "Invalid xmlrpc_value type: 0x%x", valP->_type);
    }
}



void
xmlrpc_serialize_json(xmlrpc_env *       const envP,
                      xmlrpc_value *     const valP,
                      xmlrpc_mem_block * const outP) {

    serializeValue(envP, valP, 0, outP);

    if (!envP->fault_occurred) {
        /* Append terminating NUL */
        XMLRPC_MEMBLOCK_APPEND(char, envP, outP, "", 1);
    }
}
