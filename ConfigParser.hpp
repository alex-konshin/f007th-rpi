/*
 * ConfigParser.hpp
 *
 *  Created on: November 29, 2020
 *      Author: akonshin
 */

#ifndef CONFIGPARSER_HPP_
#define CONFIGPARSER_HPP_

#include <stdio.h>
#include <stdlib.h>

#include "Logger.hpp"
#include "Utils.hpp"

typedef struct Buffer {
  char* ptr = NULL;
  size_t size = 0;
} Buffer;


//-------------------------------------------------------------
class ConfigParser : public ErrorLogger {
private:
  FILE* configFileStream = NULL;

  Buffer buffers[2];
  const char* line = NULL;
  const char* next_line = NULL;
  int next_buffer_index = 0;
  int read_linenum = 0;
  const char* valid_options_text;


  void print_error_vargs(const char* fmt, va_list vargs) {
    format_message(NULL, fmt, vargs);
    fprintf(stderr, "ERROR: %s (line #%d of file \"%s\").\n", buffer, linenum, configFilePath);
  }

public:
  int linenum = -1;
  const char* configFilePath = NULL;

  ConfigParser(const char* const valid_options_text) : ErrorLogger(LOGGER_FLAG_STDERR) {
    this->valid_options_text = valid_options_text;
    buffers[0].ptr = NULL;
    buffers[0].size = 0;
    buffers[1].ptr = NULL;
    buffers[1].size = 0;
  }


  ~ConfigParser() {
    close_file();
    line = NULL;
    next_line = NULL;
    if (configFilePath != NULL) {
      free((void*)configFilePath);
      configFilePath = NULL;
    }
  }

  void error_vargs(const char* fmt, va_list vargs) {
    print_error_vargs(fmt, vargs);
    exit(1);
  }

  void error(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    print_error_vargs(fmt, vargs);
    va_end(vargs);
    exit(1);
  }

  void print_error(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    print_error_vargs(fmt, vargs);
    va_end(vargs);
  }

  void print_valid_options() {
    fputs( valid_options_text, stderr );
  }
  void errorWithHelp(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    print_error_vargs(fmt, vargs);
    va_end(vargs);
    print_valid_options();
    fflush(stderr);
    fclose(stderr);
    exit(1);
  }

  bool open_file(const char* configFileRelativePath) {
    const char* configFilePath = realpath(configFileRelativePath, NULL);

    if (configFileStream != NULL) {
      fprintf(stderr, "ERROR: Program error - attempt to open configuration file \"%s\" when another file is already opened in this parser.\n", configFilePath);
      return false;
    }

    this->configFilePath = configFilePath;
    if (configFilePath == NULL) {
      fprintf(stderr, "ERROR: Configuration file \"%s\" does not exist.\n", configFilePath);
      return false;
    }

    configFileStream = fopen(configFilePath, "r");
    if (configFileStream == NULL) {
      fprintf(stderr, "ERROR: Cannot open configuration file \"%s\".\n", configFilePath);
      return false;
    }

    fprintf(stderr, "Reading configuration file \"%s\"...\n", configFilePath);
    linenum = read_linenum = 0;
    line = NULL;
    next_line = NULL;
    return true;
  }

  void close_file() {
    if (configFileStream != NULL) {
      fclose(configFileStream);
      configFileStream = NULL;
    }
  }

  const char* read_line() {
    if (next_line != NULL) {
      line = next_line;
      next_line = NULL;
      linenum++;
    } else {
      line = NULL;
      if (configFileStream == NULL) return NULL;

      Buffer& buffer = buffers[next_buffer_index];
      ssize_t bytesread;
      if ((bytesread = getline(&buffer.ptr, &buffer.size, configFileStream)) == -1) {
        close_file();
        return NULL;
      }
      next_buffer_index ^= 1;
      line = buffer.ptr;
      removeTralingCRLF(line);
      linenum = ++read_linenum;
    }
    return line;
  }

  const char* skipBlanks(const char*& p) {
    if (p == NULL) return NULL;
    char ch;
    while ((ch=*p) ==' ' || ch == '\t') p++;
    if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') p = NULL;
    return p;
  }


  // skip blanks but may switch to the next line if it starts with blank
  const char* skipBlanksInCommand(const char*& p) {
    char ch;
    if (p != NULL) {
      while ((ch=*p) ==' ' || ch == '\t') p++;
    }
    if (p == NULL || ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') {
      p = NULL;
      // check next line: if it starts with blank then it is continuation line
      if (next_line == NULL) {
        if (configFileStream == NULL) return NULL;

        Buffer& buffer = buffers[next_buffer_index];
        ssize_t bytesread;
        if ((bytesread = getline(&buffer.ptr, &buffer.size, configFileStream)) == -1) {
          close_file();
          return NULL;
        }
        next_buffer_index ^= 1;
        read_linenum++;
        next_line = buffer.ptr;
        removeTralingCRLF(next_line);
      }

      if ((ch=*next_line) ==' ' || ch == '\t') {
        line = p = next_line;
        next_line = NULL;
        while ((ch=*p) ==' ' || ch == '\t') p++;
        if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '#') {
          // next line is empty
          return p = NULL;
        }
        linenum++;
      }
    }
    return p;
  }

  const char* getWord(const char*& p, size_t& length) {
    length = 0;
    const char* start = skipBlanksInCommand(p);
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
  const char* getFirstWord(const char*& p, size_t& length) {
    length = 0;
    char ch;
    if (p==NULL || (ch=*p)=='\0' || ch=='\n' || ch =='\r' || ch=='#') return p = NULL;
    if (ch==' ' || ch=='\t') { // line starts with blank


      error("First character of command should not be blank");
    }

    const char* start = p;
    do {
      p++;
      if ((ch=*p)=='\0' || ch=='\n' || ch=='\r' || ch=='#') {
        length = p-start;
        p = NULL;
        return start;
      }
    } while (ch!=' ' && ch!='\t');

    length = p-start;
    return start;
  }

  void removeTralingCRLF(const char* p) {
    if (p != NULL) {
      int len = strlen(p);
      char* s = (char*)p;
      char ch;
      while (len > 0 && ((ch=s[len-1])=='\n' || ch=='\r') ) s[--len] = '\0';
    }
  }

  //-------------------------------------------------------------
  const char* getString(const char*& p, char*& buffer, size_t& bufsize, size_t& length, size_t* pkey_len) {
    length = 0;
    if (pkey_len != NULL) *pkey_len = 0;
    const char* start = skipBlanksInCommand(p);
    if (start == NULL) return p = NULL;

    char ch;
    if (*start == '"') {
      // quoted string

      int value_len = 0;
      const char* ptr = start;
      while ((ch=*++ptr) !='"') {
        if (ch == '\\') {
          ch = *++ptr;
          if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f') error("Unmatched quote ('\"')");
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
            if (!isHex(*++ptr) || !isHex(*++ptr)) error("Two digits are expected after \\x");
            break;
          case '\0':
          case '\n':
          case '\r':
          case '\f':
            error("Backslash is not allowed at the end of line");
            break;
          default:
            error("Unexpected character '%c' after backslash", ch);
          }
        } else if (ch == '\0' || ch == '\n' || ch == '\r') {
          error("Unmatched quote ('\"')");
        }
        value_len++;
      }

      ch = *++ptr;
      if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '#') {
        p = NULL;
      } else if (ch == ' ' || ch == '\t') {
        p = ptr;
      } else {
        error("Invalid quoted string");
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
          if (ch == '\0' || ch == '\n' || ch == '\r') error("Unmatched quote ('\"')");
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
            ch = getHex(ptr);
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
        if (pkey_len != NULL && ch == '=' && !include_quotes && *pkey_len == 0) {
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
          if (!inside_quotes) error("Backslash is not allowed in unquoted strings");
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
            if (!isHex(*p++) || !isHex(*p++)) error("Two digits are expected after \\x");
            break;
          case '\0':
          case '\n':
          case '\r':
          case '\f':
            error("Backslash is not allowed at the end of line");
            break;
          default:
            error("Unexpected character '%c' after backslash", ch);
          }
        }
        value_len++;
      }
    }
    if (inside_quotes) error("Unmatched quote ('\"')");
    if (!include_quotes && ch == '\0') {
      length = p-start;
      p = NULL;
      return start;
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
              ch = getHex(p);
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
  int getHex(char ch) {
    if (ch>='0' && ch<='9') return (ch-'0');
    if (ch>='a' && ch<='f') return (ch-'a')+10;
    if (ch<'A' || ch>'F') error("Invalid hex character '%c'", ch);
    return (ch-'A')+10;
  }

  char getHex(const char*& p) {
    int n = getHex(*p++);
    n <<= 4;
    int n2 = getHex(*p++);
    n |= n2;
    return (char)n;
  }

  static inline bool isHex(char ch) {
    return (ch>='0' && ch<='9') || (ch>='a' && ch<='f') || (ch>='A' && ch<='F');
  }

};


#endif /* CONFIGPARSER_HPP_ */
