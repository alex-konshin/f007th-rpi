/*
 * Utils.cpp
 *
 *  Created on: November 28, 2020
 *      Author: akonshin
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "Utils.hpp"

static const char* EMPTY_STRING = "";

//-------------------------------------------------------------
/*
   char dt[TIME2STR_BUFFER_SIZE];
   const char* str = convert_time(&data_time, dt, TIME2STR_BUFFER_SIZE, utc);
 */
const char* convert_time(time_t* data_time, char* buffer, size_t buffer_size, bool utc) {
  struct tm tm;
  if (utc) { // UTC time zone
    struct tm* ptm = gmtime_r(data_time, &tm);
    if (ptm == NULL) return NULL;
    strftime(buffer, buffer_size, "%FT%TZ", ptm); // ISO format
  } else { // local time zone
    struct tm* ptm = localtime_r(data_time, &tm);
    if (ptm == NULL) return NULL;
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S%z", ptm);
  }
  return buffer;
}

//-------------------------------------------------------------
void* resize_buffer(size_t required_buffer_size, void*& buffer, size_t& buffer_size) {
  if (buffer==NULL || buffer_size < required_buffer_size) {
    buffer = realloc(buffer, required_buffer_size);
    if (buffer != NULL) buffer_size = required_buffer_size;
  }
  return buffer;
}

//-------------------------------------------------------------
char* t2d(int t, char* buffer) {
  uint32_t dummy = 0;
  return t2d(t, buffer, dummy);
}
char* t2d(int t, char* buffer, uint32_t& length) {
  char* p = buffer+T2D_BUFFER_SIZE-1;
  *p = '\0';
  bool negative = t<0;
  if (negative) t = -t;
  int d = t%10;
  if (d!=0) {
    *--p = ('0'+d);
    *--p = '.';
  }
  t = t/10;
  do {
    d = t%10;
    *--p = ('0'+d);
    t = t/10;
  } while (t > 0);
  if (negative) *--p = '-';
  length = buffer+T2D_BUFFER_SIZE-1-p;
  return p;
}

//-------------------------------------------------------------
char* i2a(int n, char* buffer, uint32_t& length) {
  char* p = buffer+T2D_BUFFER_SIZE-1;
  *p = '\0';
  bool negative = n<0;
  if (negative) n = -n;
  do {
    int d = n%10;
    *--p = ('0'+d);
    n = n/10;
  } while (n > 0);
  if (negative) *--p = '-';
  length = buffer+T2D_BUFFER_SIZE-1-p;
  return p;
}

//-------------------------------------------------------------
const char* skipBlanks(const char*& p) {
  if (p == NULL) return NULL;
  char ch;
  while ((ch=*p) ==' ' || ch == '\t') p++;
  if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') p = NULL;
  return p;
}

//-------------------------------------------------------------
int getHex(char ch, ErrorLogger* errorLogger) {
  if (ch>='0' && ch<='9') return (ch-'0');
  if (ch>='a' && ch<='f') return (ch-'a')+10;
  if (ch<'A' || ch>'F') {
    errorLogger->error("Invalid hex character '%c'", ch);
    return -1;
  }
  return (ch-'A')+10;
}

char getHex(const char*& p, ErrorLogger* errorLogger) {
  int n = getHex(*p++, errorLogger);
  if (n < 0) return -1;
  n <<= 4;
  int n2 = getHex(*p++, errorLogger);
  if (n2 < 0) return -1;
  n |= n2;
  return (char)n;
}

static inline bool isHex(char ch) {
  return (ch>='0' && ch<='9') || (ch>='a' && ch<='f') || (ch>='A' && ch<='F');
}

//-------------------------------------------------------------
const char* getWord(const char*& p, size_t& length) {
  length = 0;
  const char* start = skipBlanks(p);
  if (start == NULL) return p = NULL;

  char ch;
  while ((ch=*p) !=' ' && ch != '\t') {
    if (ch == '\0' || ch == '\n' || ch == '\r') {
      length = p-start;
      p = NULL;
      return start;
    }
    p++;
  }

  length = p-start;
  return start;
}


#define failure(format, arg...) { if (failed != NULL) *failed = true; errorLogger->error(format, ## arg); return NULL; }

//-------------------------------------------------------------
const char* getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, size_t* pkey_len, bool* quoted, bool* failed, ErrorLogger* errorLogger) {
  length = 0;
  if (quoted != NULL) *quoted = false;
  if (pkey_len != NULL) *pkey_len = 0;
  bool error = false;
  if (failed == NULL) {
    failed = &error;
  } else {
    *failed = false;
  }
  const char* start = skipBlanks(p);
  if (start == NULL) return p = NULL;

  char ch;
  if (*start == '"') {
    // quoted string
    if (quoted != NULL) *quoted = true;

    int value_len = 0;
    const char* ptr = start;
    while ((ch=*++ptr) !='"') {
      if (ch == '\\') {
        ch = *++ptr;
        if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f') failure("Unmatched quote ('\"')");
        switch ( ch) {
        case '\\':
        case '"':
        case '\'':
        case 'n':
        case 't':
        case 'r':
        case 'f':
          break;
        case 'x':
          if (!isHex(*++ptr) || !isHex(*++ptr)) failure("Two digits are expected after \\x");
          break;
        case '\0':
        case '\n':
        case '\r':
        case '\f':
          failure("Backslash is not allowed at the end of line");
          break;
        default:
          failure("Unexpected character '%c' after backslash", ch);
        }
      } else if (ch == '\0' || ch == '\n' || ch == '\r') {
        failure("Unmatched quote ('\"')");
      }
      value_len++;
    }

    ch = *++ptr;
    if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '#') {
      p = NULL;
    } else if (ch == ' ' || ch == '\t') {
      p = ptr;
    } else {
      failure("Invalid quoted string");
    }

    size_t new_size = sizeof(char)*(value_len+1);
    if (buffer == NULL) {
      buffer = (char*)malloc(new_size);
      bufsize = new_size;
    } else if (bufsize<=new_size) {
      buffer = (char*)realloc(buffer, new_size);
      bufsize = new_size;
    }
    if (buffer == NULL) {
      fprintf(stderr, "ERROR: Out of memory\n");
      exit(1);
    }

    char* out = buffer;
    ptr = start;
    while ((ch=*++ptr) !='"') {
      if (ch == '\\') {
        ch = *++ptr;
        if (ch == '\0' || ch == '\n' || ch == '\r') failure("Unmatched quote ('\"')");
        switch ( ch) {
        case '\\':
        case '"':
        case '\'':
          break;
        case 'n':
          ch = '\n';
          break;
        case 't':
          ch = '\t';
          break;
        case 'r':
          ch = '\r';
          break;
        case 'f':
          ch = '\f';
          break;
        case 'x':
          ch = getHex(ptr, errorLogger);
          if (ch == -1) {
            if (failed != NULL) *failed = true;
            return NULL;
          }
          break;
        }
      }
      *out++ = ch;
    }
    *out++ = '\0';
    length = value_len;
    return buffer;
  }

  // started as unquoted
  bool include_quotes = false;
  bool inside_quotes = false;
  int value_len = 0;
  while ((ch=*p) != '\0' && ch != '\r' && ch != '\n' && ch!='\f') {
    if (!inside_quotes) {
      if (ch == ' ' || ch == '\t' || ch == '#') break;
      if (ch == '=' && !include_quotes && pkey_len != NULL && *pkey_len == 0) {
        *pkey_len = p-start;
        if (*pkey_len == 0) *pkey_len = -1; // missing key but the argument still may be valid
      }
    }
    p++;

    if (ch == '"')  {
      inside_quotes = !inside_quotes;
      include_quotes = true;
    } else {
      if (ch == '\\') {
        if (!inside_quotes) failure("Backslash is not allowed in unquoted strings");
        ch = *p++;
        switch ( ch) {
        case '\\':
        case '"':
        case '\'':
        case 'n':
        case 't':
        case 'r':
        case 'f':
          break;
        case 'x':
          if (!isHex(*p++) || !isHex(*p++)) failure("Two digits are expected after \\x");
          break;
        case '\0':
        case '\n':
        case '\r':
        case '\f':
          failure("Backslash is not allowed at the end of line");
          break;
        default:
          failure("Unexpected character '%c' after backslash", ch);
        }
      }
      value_len++;
    }
  }
  if (inside_quotes) failure("Unmatched quote ('\"')");
  if (!include_quotes && ch == '\0') {
    length = p-start;
    p = NULL;
    return start;
  }
  if (quoted != NULL) *quoted = include_quotes;
  size_t new_size = sizeof(char)*(value_len+1);
  if (buffer == NULL) {
    buffer = (char*)malloc(new_size);
    bufsize = new_size;
  } else if (bufsize<=new_size) {
    buffer = (char*)realloc(buffer, new_size);
    bufsize = new_size;
  }
  if (buffer == NULL) {
    fprintf(stderr, "ERROR: Out of memory\n");
    exit(1);
  }
  if (!include_quotes) {
    strncpy(buffer, start, value_len);
    buffer[value_len] = '\0';
  } else {
    inside_quotes = false;
    p = start;
    char* pout = buffer;
    while ((ch=*p++) != '\0' && ch != '\r' && ch != '\n' && ch!='\f') {
      if (!inside_quotes && (ch == ' ' || ch == '\t' || ch == '#')) break;
      if (ch == '"')  {
        inside_quotes = !inside_quotes;
      } else {
        if (ch == '\\') {
          ch = *p++;
          switch ( ch) {
          case '\\':
          case '"':
          case '\'':
            break;
          case 'n':
            ch = '\n';
            break;
          case 't':
            ch = '\t';
            break;
          case 'r':
            ch = '\r';
            break;
          case 'f':
            ch = '\f';
            break;
          case 'x':
            ch = getHex(p, errorLogger);
            if (ch == -1) {
              if (failed != NULL) *failed = true;
              return NULL;
            }
            break;
          }
        }
        *pout++ = ch;
      }
    }
    *pout = '\0';
    value_len = pout-buffer;
  }

  if (ch=='\0' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '#') p = NULL;
  length = value_len;
  return buffer;
}


//-------------------------------------------------------------
int16_t readInt(const char*& p) {
  int16_t n = 0;

  char ch;
  while ((ch=*p)==' ') p++;

  if (ch=='\0' || ch=='\n' || ch=='\r') return -1;

  do {
    n = n*10+(ch-'0');
    ch = *++p;
  } while (ch>='0' && ch<='9');

  if (ch==',') p++;
  return n;
}

//-------------------------------------------------------------
uint32_t getUnsigned(const char*& p, ErrorLogger* errorLogger) {
  if (skipBlanks(p) == NULL) errorLogger->error("Missed unsigned integer argument");

  uint32_t result = 0;

  char ch = *p;
  if (ch>='0' && ch<='9') {
    result = (ch-'0');
  } else {
    errorLogger->error("Unsigned integer argument was expected");
  }

  if ( result == 0 && *(p+1) == 'x') { // hex number
    p += 2;
    result = getHex(*p, errorLogger); // must be at least one hex digit
    for ( int i=0; i<7; i++) {
      ch = *++p;
      if (ch == ' ' || ch == '\t' ) return result;
      if (ch == '\0' || ch == '#' || ch == '\r' || ch == '\n' ) {
        p = NULL;
        return result;
      }
      result <<= 4;
      result |= getHex(ch, errorLogger);
    }
    ch = *++p;
    if (ch == ' ' || ch == '\t' ) return result;
    if (ch != '\0' && ch != '#' && ch != '\r' && ch != '\n' ) errorLogger->error("Invalid hex number");
    p = NULL;
    return result;
  }

  while ( (ch=*++p)!='\0' && ch != '#' && ch != '\r' && ch != '\n') {
    if (ch == ' ' || ch == '\t' ) return result;
    if (ch>='0' && ch<='9') {
      result = result*10 + (ch-'0');
    } else {
      errorLogger->error("Unsigned integer argument was expected");
    }
  }
  p = NULL;
  return result;
}

//-------------------------------------------------------------
const char* clone(const char* str) {
  if (str == NULL) return NULL;
  int length = strlen(str);
  if (length == 0) return EMPTY_STRING;
  char* result = (char*)malloc((length+1)*sizeof(char));
  strcpy(result, str);
  return result;
}

//-------------------------------------------------------------
const char* make_str(const char* str, size_t length) {
  if (str == NULL) return NULL;
  if (length == 0) return EMPTY_STRING;
  char* result = (char*)malloc((length+1)*sizeof(char));
  strcpy(result, str);
  result[length] = '\0';
  return result;
}



//-------------------------------------------------------------
/*
static void start_process() {
  char *const parmList[] = {"/bin/ls", "-l", "/u/userid/dirname", NULL};

  pid_t pid;
  if ((pid = fork()) == -1) {
     perror("fork() error"); // FIXME

  } else if (pid == 0) {
     execvp("ls", parmList);
     printf("Return not expected. Must be an execvp() error.n");
  }
}
*/
