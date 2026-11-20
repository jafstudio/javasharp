#include <algorithm>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace js {

const std::string VERSION = "1.0.0";
const std::string PROGNAME = "JavaSharp";

inline bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
inline bool endsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
inline std::string toLowerS(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
inline std::string trimS(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
inline std::string replaceAllS(std::string s, const std::string& f, const std::string& r) {
    if (f.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(f, pos)) != std::string::npos) {
        s.replace(pos, f.size(), r);
        pos += r.size();
    }
    return s;
}
inline std::vector<std::string> splitStr(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == d) {
            out.push_back(cur);
            cur.clear();
        } else cur += c;
    }
    out.push_back(cur);
    return out;
}
inline std::string joinV(const std::vector<std::string>& v, const std::string& d) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i) out += d;
        out += v[i];
    }
    return out;
}

inline bool isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_' || c == '$'; }
inline bool isIdentPart(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '$'; }
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
inline bool isHexDigit(char c) { return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

inline const std::set<std::string>& csKeywords() {
    static const std::set<std::string> kw = {
        "abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char",
        "checked", "class", "const", "continue", "decimal", "default", "delegate",
        "do", "double", "else", "enum", "event", "explicit", "extern", "false",
        "finally", "fixed", "float", "for", "foreach", "goto", "if", "implicit",
        "in", "int", "interface", "internal", "is", "lock", "long", "namespace",
        "new", "null", "object", "operator", "out", "override", "params", "private",
        "protected", "public", "readonly", "ref", "return", "sbyte", "sealed",
        "short", "sizeof", "stackalloc", "static", "string", "struct", "switch",
        "this", "throw", "true", "try", "typeof", "uint", "ulong", "unchecked",
        "unsafe", "ushort", "using", "virtual", "void", "volatile", "while"};
    return kw;
}

inline std::string csIdent(const std::string& n) {
    if (csKeywords().count(n)) return "@" + n;
    return n;
}

inline int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline void appendUtf8(std::string& out, unsigned int value) {
    if (value <= 0x7F) out += static_cast<char>(value);
    else if (value <= 0x7FF) {
        out += static_cast<char>(0xC0 | (value >> 6));
        out += static_cast<char>(0x80 | (value & 0x3F));
    } else if (value <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (value >> 12));
        out += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (value & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (value >> 18));
        out += static_cast<char>(0x80 | ((value >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((value >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (value & 0x3F));
    }
}

inline bool readUnicodeEscape(const std::string& raw, size_t slash, size_t& end, unsigned int& value) {
    size_t j = slash + 1;
    if (j >= raw.size() || raw[j] != 'u') return false;
    while (j < raw.size() && raw[j] == 'u') j++;
    if (j + 4 > raw.size()) return false;
    value = 0;
    for (size_t k = 0; k < 4; k++) {
        int h = hexVal(raw[j + k]);
        if (h < 0) return false;
        value = (value << 4) | static_cast<unsigned int>(h);
    }
    end = j + 3;
    return true;
}

inline std::string decodeJavaEscapes(const std::string& raw) {
    std::string out;
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        if (c != '\\') {
            out += c;
            continue;
        }
        if (i + 1 >= raw.size()) break;
        char e = raw[++i];
        if (e == 'n') out += '\n';
        else if (e == '\n') {}
        else if (e == '\r') {
            if (i + 1 < raw.size() && raw[i + 1] == '\n') i++;
        } else if (e == 't') out += '\t';
        else if (e == 'r') out += '\r';
        else if (e == 'b') out += '\b';
        else if (e == 'f') out += '\f';
        else if (e == 's') out += ' ';
        else if (e == '"') out += '"';
        else if (e == '\'') out += '\'';
        else if (e == '\\') out += '\\';
        else if (e == 'u') {
            size_t end = i;
            unsigned int value = 0;
            if (!readUnicodeEscape(raw, i - 1, end, value)) {
                out += 'u';
                continue;
            }
            i = end;
            if (value >= 0xD800 && value <= 0xDBFF) {
                size_t lowEnd = i + 1;
                unsigned int low = 0;
                if (i + 1 < raw.size() && raw[i + 1] == '\\' &&
                    readUnicodeEscape(raw, i + 1, lowEnd, low) && low >= 0xDC00 && low <= 0xDFFF) {
                    value = 0x10000 + ((value - 0xD800) << 10) + (low - 0xDC00);
                    i = lowEnd;
                }
            }
            appendUtf8(out, value);
        } else if (e >= '0' && e <= '7') {
            int value = e - '0';
            int maxDigits = e <= '3' ? 3 : 2;
            int count = 1;
            while (count < maxDigits && i + 1 < raw.size() &&
                   raw[i + 1] >= '0' && raw[i + 1] <= '7') {
                value = value * 8 + (raw[++i] - '0');
                count++;
            }
            out += static_cast<char>(value);
        } else {
            out += e;
        }
    }
    return out;
}

inline int utf16UnitCount(const std::string& value) {
    int count = 0;
    for (size_t i = 0; i < value.size();) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x80) {
            i++;
            count++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < value.size()) {
            i += 2;
            count++;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < value.size()) {
            i += 3;
            count++;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < value.size()) {
            i += 4;
            count += 2;
        } else {
            i++;
            count++;
        }
    }
    return count;
}

inline std::string escapeCsStr(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xED && i + 2 < s.size() &&
            static_cast<unsigned char>(s[i + 1]) >= 0xA0 &&
            static_cast<unsigned char>(s[i + 1]) <= 0xBF &&
            (static_cast<unsigned char>(s[i + 2]) & 0xC0) == 0x80) {
            unsigned int value = ((c & 0x0F) << 12) |
                                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\\u%04X", value);
            out += buf;
            i += 3;
            continue;
        }
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\0') out += "\\0";
        else if (c < 32) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\\u%04X", c);
            out += buf;
        } else out += static_cast<char>(c);
        i++;
    }
    return out;
}

inline std::string escapeCsChar(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xED && i + 2 < s.size() &&
            static_cast<unsigned char>(s[i + 1]) >= 0xA0 &&
            static_cast<unsigned char>(s[i + 1]) <= 0xBF &&
            (static_cast<unsigned char>(s[i + 2]) & 0xC0) == 0x80) {
            unsigned int value = ((c & 0x0F) << 12) |
                                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\\u%04X", value);
            out += buf;
            i += 3;
            continue;
        }
        if (c == '\'') out += "\\'";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\0') out += "\\0";
        else if (c < 32) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\\u%04X", c);
            out += buf;
        } else out += static_cast<char>(c);
        i++;
    }
    return out;
}

struct FmtXlate {
    std::string fmt;
    std::vector<int> tags;
};

inline FmtXlate translatePrintfFmt(const std::string& in) {
    FmtXlate res;
    std::string& out = res.fmt;
    size_t i = 0;
    int nextArg = 0;
    int previousArg = -1;
    auto setTag = [&](int index, int tag) {
        if (index < 0) return;
        if ((size_t)index >= res.tags.size()) res.tags.resize((size_t)index + 1, 0);
        res.tags[(size_t)index] = tag;
    };
    while (i < in.size()) {
        if (in[i] != '%') {
            if (in[i] == '{') out += "{{";
            else if (in[i] == '}') out += "}}";
            else out += in[i];
            i++;
            continue;
        }
        size_t j = i + 1;
        if (j < in.size() && in[j] == '%') {
            out += '%';
            i = j + 1;
            continue;
        }
        if (j < in.size() && in[j] == 'n') {
            out += '\n';
            i = j + 1;
            continue;
        }
        size_t digitsStart = j;
        while (j < in.size() && isDigit(in[j])) j++;
        int explicitArg = -1;
        if (j > digitsStart && j < in.size() && in[j] == '$') {
            try {
                explicitArg = std::stoi(in.substr(digitsStart, j - digitsStart)) - 1;
            } catch (...) {
                explicitArg = -1;
            }
            j++;
        } else {
            j = digitsStart;
        }
        bool left = false;
        bool zero = false;
        bool plus = false;
        bool space = false;
        bool grouping = false;
        bool parentheses = false;
        while (j < in.size() && std::strchr("-#+ 0,(<", in[j])) {
            if (in[j] == '-') left = true;
            else if (in[j] == '0') zero = true;
            else if (in[j] == '+') plus = true;
            else if (in[j] == ' ') space = true;
            else if (in[j] == ',') grouping = true;
            else if (in[j] == '(') parentheses = true;
            j++;
        }
        std::string width;
        while (j < in.size() && isDigit(in[j])) width += in[j++];
        std::string precision;
        if (j < in.size() && in[j] == '.') {
            j++;
            while (j < in.size() && isDigit(in[j])) precision += in[j++];
        }
        bool dateTime = false;
        if (j < in.size() && (in[j] == 't' || in[j] == 'T')) {
            dateTime = true;
            j++;
        }
        if (j >= in.size()) {
            out += in.substr(i);
            break;
        }
        char conv = in[j];
        i = j + 1;
        int index = explicitArg;
        if (index < 0) {
            if (in.substr(digitsStart, j - digitsStart).find('<') != std::string::npos && previousArg >= 0)
                index = previousArg;
            else index = nextArg++;
        }
        previousArg = index;
        if (dateTime) {
            out += "{" + std::to_string(index) + "}";
            continue;
        }
        std::string spec;
        if (conv == 'd') {
            if (zero && !width.empty()) spec = ":D" + width;
            else if (grouping) spec = ":N0";
            else if (plus) spec = ":+0;-0;0";
            else if (space) spec = ": 0;-0; 0";
            else if (parentheses) spec = ":0;(0);0";
        } else if (conv == 'f' || conv == 'F') {
            spec = ":F" + (precision.empty() ? "6" : precision);
        } else if (conv == 'e' || conv == 'E') {
            spec = ":" + std::string(1, conv == 'E' ? 'E' : 'e') +
                   (precision.empty() ? "6" : precision);
        } else if (conv == 'g' || conv == 'G') {
            spec = ":" + std::string(1, conv) + (precision.empty() ? "" : precision);
        } else if (conv == 'x' || conv == 'X') {
            spec = ":" + std::string(1, conv);
        } else if (conv == 'h' || conv == 'H') {
            spec = ":" + std::string(1, conv == 'H' ? 'X' : 'x');
        } else if (conv == 'o') {
            spec.clear();
            setTag(index, 2);
        } else if (conv == 's' || conv == 'S' || conv == 'c' || conv == 'C' || conv == 'a' || conv == 'A') {
            spec.clear();
        } else if (conv == 'b' || conv == 'B') {
            spec.clear();
            setTag(index, 1);
        } else {
            out += "%";
            out += conv;
            continue;
        }
        std::string slot = "{" + std::to_string(std::max(0, index));
        if (!width.empty() && !zero) {
            if (left) slot += ",-" + width;
            else slot += "," + width;
        }
        slot += spec + "}";
        out += slot;
    }
    return res;
}

inline std::string translateDatePattern(const std::string& p) {
    std::string out;
    size_t i = 0;
    while (i < p.size()) {
        char c = p[i];
        if (c == '\'') {
            size_t j = i + 1;
            std::string lit;
            if (j < p.size() && p[j] == '\'') {
                out += "'";
                i = j + 1;
                continue;
            }
            while (j < p.size() && p[j] != '\'') lit += p[j++];
            if (j < p.size()) j++;
            out += "'" + lit + "'";
            i = j;
            continue;
        }
        size_t j = i;
        while (j < p.size() && p[j] == c) j++;
        size_t n = j - i;
        if (c == 'y' || c == 'Y') out += std::string(n, 'y');
        else if (c == 'M') out += std::string(n, 'M');
        else if (c == 'L') out += std::string(std::min<size_t>(n, 4), 'M');
        else if (c == 'd') out += std::string(n, 'd');
        else if (c == 'D') out += "";
        else if (c == 'H') out += std::string(n, 'H');
        else if (c == 'k') out += std::string(n, 'H');
        else if (c == 'h') out += std::string(n, 'h');
        else if (c == 'K') out += std::string(n, 'h');
        else if (c == 'm') out += std::string(n, 'm');
        else if (c == 's') out += std::string(n, 's');
        else if (c == 'S') {
            if (n >= 3) out += "fff";
            else if (n == 2) out += "ff";
            else out += "f";
        } else if (c == 'a') out += "tt";
        else if (c == 'E') out += (n >= 4 ? "dddd" : "ddd");
        else if (c == 'z') out += "zzz";
        else if (c == 'Z' || c == 'X') out += "zzz";
        else if (c == 'G') out += "";
        else if (c == 'w' || c == 'W' || c == 'F' || c == 'u') out += "";
        else out += std::string(n, c);
        i = j;
    }
    return out;
}

inline const std::map<std::string, std::string>& wrapperMap() {
    static const std::map<std::string, std::string> m = {
        {"Integer", "int"}, {"Long", "long"}, {"Short", "short"}, {"Byte", "byte"},
        {"Double", "double"}, {"Float", "float"}, {"Boolean", "bool"}, {"Character", "char"},
        {"Number", "double"}
    };
    return m;
}

inline const std::map<std::string, std::string>& typeMapTable() {
    static const std::map<std::string, std::string> m = {
        {"Object", "object"}, {"String", "string"}, {"CharSequence", "string"},
        {"Math", "Math"}, {"StrictMath", "Math"},
        {"StringBuilder", "StringBuilder"}, {"StringBuffer", "StringBuilder"},
        {"List", "List"}, {"ArrayList", "List"}, {"AbstractList", "List"}, {"Vector", "List"},
        {"AbstractSequentialList", "List"},
        {"LinkedList", "LinkedList"}, {"Stack", "Stack"}, {"Queue", "LinkedList"},
        {"Deque", "LinkedList"}, {"ArrayDeque", "LinkedList"}, {"PriorityQueue", "List"},
        {"AbstractQueue", "LinkedList"},
        {"Set", "HashSet"}, {"HashSet", "HashSet"}, {"LinkedHashSet", "HashSet"},
        {"TreeSet", "SortedSet"}, {"SortedSet", "SortedSet"}, {"NavigableSet", "SortedSet"},
        {"AbstractSet", "HashSet"}, {"EnumSet", "HashSet"},
        {"Map", "Dictionary"}, {"HashMap", "Dictionary"}, {"LinkedHashMap", "Dictionary"},
        {"TreeMap", "SortedDictionary"}, {"SortedMap", "SortedDictionary"},
        {"NavigableMap", "SortedDictionary"}, {"Hashtable", "Dictionary"},
        {"AbstractMap", "Dictionary"}, {"WeakHashMap", "Dictionary"},
        {"Properties", "Dictionary<string,string>"},
        {"Collection", "ICollection"}, {"AbstractCollection", "ICollection"},
        {"Iterable", "IEnumerable"}, {"Iterator", "IEnumerator"},
        {"Enumeration", "IEnumerable"}, {"Spliterator", "IEnumerable"},
        {"Stream", "IEnumerable"}, {"IntStream", "IEnumerable"}, {"LongStream", "IEnumerable"},
        {"DoubleStream", "IEnumerable"},
        {"Map.Entry", "KeyValuePair"}, {"Entry", "KeyValuePair"},
        {"Map.Entry::getKey", ""}, {"UUID", "Guid"},
        {"Pattern", "Regex"}, {"Matcher", "Match"}, {"MatchResult", "Match"},
        {"Locale", "CultureInfo"}, {"Charset", "Encoding"}, {"StandardCharsets", "Encoding"},
        {"BigDecimal", "decimal"}, {"BigInteger", "BigInteger"},
        {"Random", "Random"}, {"SecureRandom", "Random"}, {"Thread", "Thread"},
        {"ThreadGroup", "Thread"},
        {"Date", "DateTime"}, {"Calendar", "DateTime"},
        {"LocalDate", "DateOnly"}, {"LocalTime", "TimeOnly"}, {"LocalDateTime", "DateTime"},
        {"Instant", "DateTimeOffset"}, {"Duration", "TimeSpan"}, {"Period", "TimeSpan"},
        {"ZonedDateTime", "DateTimeOffset"}, {"OffsetDateTime", "DateTimeOffset"},
        {"ZoneId", "TimeZoneInfo"}, {"ZoneOffset", "TimeZoneInfo"},
        {"SimpleDateFormat", "DateTimeFormat"},
        {"Scanner", "__Scanner"},
        {"Path", "string"},
        {"BufferedReader", "StreamReader"}, {"BufferedWriter", "StreamWriter"},
        {"FileReader", "StreamReader"}, {"FileWriter", "StreamWriter"},
        {"InputStreamReader", "StreamReader"}, {"OutputStreamWriter", "StreamWriter"},
        {"StringReader", "StringReader"}, {"StringWriter", "StringWriter"},
        {"PrintWriter", "StreamWriter"}, {"PrintStream", "TextWriter"},
        {"InputStream", "Stream"}, {"OutputStream", "Stream"},
        {"FileInputStream", "FileStream"}, {"FileOutputStream", "FileStream"},
        {"Reader", "TextReader"}, {"Writer", "TextWriter"},
        {"Cloneable", "ICloneable"}, {"Comparable", "IComparable"},
        {"Comparator", "Comparison"}, {"Runnable", "Action"}, {"Callable", "Func"},
        {"AutoCloseable", "IDisposable"}, {"Readable", "IDisposable"},
        {"Supplier", "Func"}, {"Consumer", "Action"}, {"Predicate", "Predicate"},
        {"Function", "Func"}, {"BiFunction", "Func"}, {"BiConsumer", "Action"},
        {"BiPredicate", "Func"}, {"UnaryOperator", "Func"}, {"BinaryOperator", "Func"},
        {"IntSupplier", "Func"}, {"LongSupplier", "Func"}, {"DoubleSupplier", "Func"},
        {"BooleanSupplier", "Func"}, {"IntPredicate", "Predicate"},
        {"LongPredicate", "Predicate"}, {"DoublePredicate", "Predicate"},
        {"IntConsumer", "Action"}, {"LongConsumer", "Action"}, {"DoubleConsumer", "Action"},
        {"IntFunction", "Func"}, {"LongFunction", "Func"}, {"DoubleFunction", "Func"},
        {"ToIntFunction", "Func"}, {"ToLongFunction", "Func"}, {"ToDoubleFunction", "Func"},
        {"IntUnaryOperator", "Func"}, {"LongUnaryOperator", "Func"},
        {"DoubleUnaryOperator", "Func"}, {"IntBinaryOperator", "Func"},
        {"LongBinaryOperator", "Func"}, {"DoubleBinaryOperator", "Func"},
        {"ObjIntConsumer", "Action"}, {"ObjLongConsumer", "Action"},
        {"ObjDoubleConsumer", "Action"},
        {"IntToLongFunction", "Func"}, {"IntToDoubleFunction", "Func"},
        {"DoubleToIntFunction", "Func"}, {"DoubleToLongFunction", "Func"},
        {"LongToIntFunction", "Func"}, {"LongToDoubleFunction", "Func"},
        {"Exception", "Exception"}, {"RuntimeException", "Exception"},
        {"IllegalArgumentException", "ArgumentException"},
        {"IllegalStateException", "InvalidOperationException"},
        {"NullPointerException", "NullReferenceException"},
        {"IndexOutOfBoundsException", "IndexOutOfRangeException"},
        {"ArrayIndexOutOfBoundsException", "IndexOutOfRangeException"},
        {"StringIndexOutOfBoundsException", "ArgumentOutOfRangeException"},
        {"ClassCastException", "InvalidCastException"},
        {"NumberFormatException", "FormatException"},
        {"ArithmeticException", "ArithmeticException"},
        {"UnsupportedOperationException", "NotSupportedException"},
        {"ConcurrentModificationException", "InvalidOperationException"},
        {"NoSuchElementException", "InvalidOperationException"},
        {"InterruptedException", "ThreadInterruptedException"},
        {"InputMismatchException", "FormatException"},
        {"CloneNotSupportedException", "Exception"},
        {"NoSuchMethodException", "MissingMethodException"},
        {"NegativeArraySizeException", "OverflowException"},
        {"IOException", "IOException"},
        {"FileNotFoundException", "FileNotFoundException"},
        {"EOFException", "EndOfStreamException"},
        {"TimeoutException", "TimeoutException"},
        {"SQLException", "Exception"}, {"ParseException", "FormatException"},
        {"StackOverflowError", "StackOverflowException"},
        {"OutOfMemoryError", "OutOfMemoryException"},
        {"AssertionError", "Exception"}, {"Error", "Exception"}, {"Throwable", "Exception"},
        {"ExceptionInInitializerError", "Exception"},
        {"OptionalInt", "int?"}, {"OptionalLong", "long?"}, {"OptionalDouble", "double?"},
        {"CountDownLatch", "object"}
    };
    return m;
}

inline const std::set<std::string>& primitiveTypes() {
    static const std::set<std::string> s = {"int", "long", "short", "byte", "char", "boolean",
                                            "float", "double", "void"};
    return s;
}

inline std::string mapPrim(const std::string& t) {
    if (t == "boolean") return "bool";
    if (t == "byte") return "byte";
    return t;
}

inline std::string mapJavaType(const std::string& jname, bool genArg) {
    if (primitiveTypes().count(jname)) return mapPrim(jname);
    if (jname == "var") return "var";
    auto& wm = wrapperMap();
    auto wit = wm.find(jname);
    if (wit != wm.end()) return genArg ? wit->second : wit->second + "?";
    auto& tm = typeMapTable();
    auto it = tm.find(jname);
    if (it != tm.end()) return it->second;
    auto fit = tm.find(jname);
    if (fit != tm.end()) return fit->second;
    return "";
}

inline std::string mapJavaTypeFqn(const std::string& fqn, bool genArg) {
    std::string r = mapJavaType(fqn, genArg);
    if (!r.empty()) return r;
    size_t dot = fqn.rfind('.');
    std::string simple = dot == std::string::npos ? fqn : fqn.substr(dot + 1);
    if (startsWith(simple, "Map$")) simple = "Map.Entry";
    r = mapJavaType(simple, genArg);
    if (!r.empty()) return r;
    return "";
}

inline std::string csNamespaceForJavaPkg(const std::string& pkg) {
    static const std::map<std::string, std::string> m = {
        {"java.util", "System.Collections.Generic"},
        {"java.util.function", "System"},
        {"java.util.stream", "System.Linq"},
        {"java.lang", "System"},
        {"java.math", "System.Numerics"},
        {"java.io", "System.IO"},
        {"java.nio.file", "System.IO"},
        {"java.nio.charset", "System.Text"},
        {"java.text", "System.Globalization"},
        {"java.time", "System"},
        {"java.net", "System.Net"},
        {"java.awt", "System.Drawing"},
        {"javax.swing", "System.Windows.Forms"}
    };
    auto it = m.find(pkg);
    if (it != m.end()) return it->second;
    return pkg;
}

inline bool isNumericCs(const std::string& t) {
    return t == "int" || t == "long" || t == "short" || t == "byte" || t == "char" ||
           t == "float" || t == "double" || t == "decimal" || t == "uint" || t == "ulong" ||
           t == "sbyte";
}
inline bool isIntCs(const std::string& t) {
    return t == "int" || t == "long" || t == "short" || t == "byte" || t == "char" ||
           t == "uint" || t == "ulong" || t == "sbyte";
}

inline int csRank(const std::string& t) {
    if (t == "sbyte" || t == "byte") return 1;
    if (t == "short" || t == "ushort") return 2;
    if (t == "int" || t == "uint") return 3;
    if (t == "long" || t == "ulong") return 4;
    if (t == "char") return 3;
    if (t == "float") return 5;
    if (t == "double") return 6;
    if (t == "decimal") return 7;
    return 0;
}

inline bool needsCast(const std::string& from, const std::string& to) {
    int f = csRank(from), t2 = csRank(to);
    if (f == 0 || t2 == 0) return false;
    if (from == "char" && (to == "float" || to == "double" || to == "long" || to == "decimal")) return false;
    if (to == "char") return true;
    if (from == "char" && t2 < 3) return true;
    return f > t2;
}

inline bool hasMod(const std::vector<std::string>& mods, const std::string& m) {
    return std::find(mods.begin(), mods.end(), m) != mods.end();
}
struct Expr;
struct Stmt;
struct Member;
struct Decl;

struct Ann {
    std::string name;
    bool hasArgs = false;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> args;
    std::vector<std::unique_ptr<Expr>> pargs;
    int line = 0;
};

enum class TT { Id, Kw, Int, Flt, Chr, Str, TBlk, Null, Bool, Punct, End };

struct Token {
    TT t = TT::Id;
    std::string s;
    int line = 0, col = 0;
};

struct Comment {
    std::string text;
    int line = 0;
    bool block = false;
};

struct TypeRef {
    std::string name;
    std::vector<TypeRef> args;
    int dims = 0;
    bool wildcard = false;
    bool isVar = false;
    bool diamond = false;

    bool empty() const { return name.empty() && dims == 0 && !wildcard; }
    std::string text() const {
        std::string r = wildcard ? (name.empty() ? "?" : "? extends " + name) : name;
        if (!args.empty()) {
            r += "<";
            for (size_t i = 0; i < args.size(); i++) {
                if (i) r += ", ";
                r += args[i].text();
            }
            r += ">";
        }
        for (int i = 0; i < dims; i++) r += "[]";
        return r;
    }
};

struct TP {
    std::string name;
    TypeRef bound;
    bool hasBound = false;
};

struct EnumConst {
    std::string name;
    std::vector<std::unique_ptr<Expr>> args;
    std::unique_ptr<Decl> body;
    int line = 0;
};

struct AnonBody {
    TypeRef base;
    std::vector<Member> members;
    int line = 0;
};

struct CaseArm {
    bool isDefault = false;
    bool arrow = false;
    std::vector<std::unique_ptr<Expr>> labels;
    std::vector<TypeRef> patLabels;
    std::string patName;
    bool nullLabel = false;
    std::unique_ptr<Expr> guard;
    std::vector<std::unique_ptr<Stmt>> body;
    std::unique_ptr<Expr> value;
    bool isBlockVal = false;
    int line = 0;
};

enum class EK {
    Ident, Field, Call, New, NewArr, ArrIdx, Lit, Cast, Bin, Un, Post, Assign,
    Ternary, Lambda, MRef, InstOf, SwitchE, This, ClassLit, Paren, TypeE, CtorCall
};

struct Expr {
    EK k = EK::Ident;
    std::string op;
    std::string name;
    std::string val;
    TT lt = TT::Int;
    bool rawLit = false;
    bool postfix = false;
    bool diamond = false;
    bool implArr = false;
    TypeRef ty;
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<std::unique_ptr<Expr>> dims;
    std::unique_ptr<Expr> obj, a, b, c;
    std::vector<TypeRef> targs;
    std::vector<std::pair<TypeRef, std::string>> lparams;
    std::unique_ptr<Expr> lbody;
    std::vector<std::unique_ptr<Stmt>> lblock;
    bool lbodyIsBlock = false;
    std::string patName;
    std::vector<std::pair<TypeRef, std::string>> patComps;
    std::vector<CaseArm> cases;
    std::unique_ptr<AnonBody> anon;
    int line = 0;
};

enum class SK {
    Block, ExprStmt, LocalVar, If, While, DoWhile, For, Foreach, Break, Continue,
    Return, Throw, Try, Switch, Sync, Assert, Empty, Label, LocalClass
};

struct Stmt {
    SK k = SK::Empty;
    std::unique_ptr<Expr> e;
    std::unique_ptr<Expr> e2;
    std::unique_ptr<Expr> subject;
    std::vector<std::unique_ptr<Expr>> initExprs, updateExprs;
    std::unique_ptr<Stmt> body, body2;
    std::vector<std::unique_ptr<Stmt>> block;
    TypeRef ty;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<Expr>> inits;
    std::vector<int> dims;
    std::vector<std::pair<TypeRef, std::string>> resources;
    std::vector<std::unique_ptr<Expr>> resInits;
    std::string label;
    struct Catch {
        std::vector<TypeRef> types;
        std::string var;
        std::unique_ptr<Stmt> body;
    };
    std::vector<Catch> catches;
    std::unique_ptr<Stmt> fin;
    std::unique_ptr<Decl> lclass;
    std::vector<CaseArm> arms;
    bool arrowSw = false;
    bool isExprSw = false;
    bool isYield = false;
    int line = 0;
};

struct Member {
    int mk = 0;
    std::string name;
    std::vector<std::string> mods;
    std::vector<Ann> anns;
    TypeRef ty;
    std::vector<std::pair<TypeRef, std::string>> params;
    bool varargs = false;
    std::vector<TP> tparams;
    std::vector<std::string> names;
    std::vector<int> fdims;
    std::vector<std::unique_ptr<Expr>> finits;
    std::unique_ptr<Stmt> body;
    std::vector<TypeRef> throws;
    bool isStaticInit = false;
    bool compactCtor = false;
    std::unique_ptr<Decl> nested;
    std::vector<std::unique_ptr<Expr>> baseArgs;
    int line = 0;
};

struct Decl {
    int dk = 0;
    std::vector<std::string> mods;
    std::vector<Ann> anns;
    std::string name;
    std::vector<TP> tparams;
    TypeRef extends;
    std::vector<TypeRef> impls;
    std::vector<EnumConst> consts;
    std::vector<Member> members;
    int line = 0;
};

struct Unit {
    std::string package;
    std::vector<std::pair<std::string, bool>> imports;
    std::vector<std::unique_ptr<Decl>> types;
    int line = 0;
};

struct Ctx {
    std::vector<Comment> comments;
    size_t nextComment = 0;
    std::set<std::string> usings;
    bool needLinq = false, needText = false, needIO = false, needRegex = false;
    bool needDiag = false, needCompilerServices = false, needGlobalization = false;
    bool needNumerics = false, needThreading = false, needCollectionsSpec = false;
    bool needScanner = false;
    bool needReflection = false;
    std::map<std::string, Decl*> typeByName;
    std::map<std::string, std::string> staticImports;
    bool needStaticMath = false;
    bool staticCollectionsAll = false;
    std::set<std::string> enumClassTypes;
    std::set<std::string> simpleEnums;
    std::map<std::string, std::string> enumConstOwner;
    std::set<std::string> recordTypes;
    std::map<std::string, std::vector<std::string>> recordComps;
    std::set<std::string> samDelegates;
    std::map<std::string, std::string> samMethodNames;
    std::map<std::string, std::set<std::string>> virtualM;
    std::map<std::string, std::set<std::string>> overrideM;
    std::map<std::string, std::map<std::string, TypeRef>> methodReturns;
    std::map<std::string, std::vector<TypeRef>> methodParamTypes;
    std::map<std::string, std::set<std::string>> classFieldNames;
    std::map<std::string, TypeRef> classFieldType;
    std::set<std::string> staticFields;
    std::set<std::string> staticMethods;
    std::set<std::string> instanceFields;
    std::set<std::string> instanceMethods;
    std::vector<std::string> errors;
    bool anyMain = false;
    int mainCount = 0;

    void error(int line, const std::string& msg) {
        errors.push_back("line " + std::to_string(line) + ": " + msg);
    }
};

struct TypeInfo {
    std::string name;
    std::vector<TypeInfo> args;
    int dims = 0;
    bool known = false;

    TypeInfo elem() const {
        TypeInfo r;
        if (known && dims > 0) {
            r = *this;
            r.dims--;
            return r;
        }
        if (known && !args.empty()) return args[0];
        return r;
    }
    std::string text() const {
        if (!known) return "?";
        std::string r = name;
        if (!args.empty()) {
            r += "<";
            for (size_t i = 0; i < args.size(); i++) {
                if (i) r += ", ";
                r += args[i].text();
            }
            r += ">";
        }
        for (int i = 0; i < dims; i++) r += "[]";
        return r;
    }
};

struct VarInfo {
    std::string name;
    TypeInfo ty;
};
struct Lex {
    const std::string& src;
    size_t i = 0;
    int line = 1, col = 1;
    std::vector<Token> toks;
    std::vector<Comment> comments;
    Ctx& ctx;

    explicit Lex(const std::string& s, Ctx& c) : src(s), ctx(c) {}

    char peek(size_t off = 0) const { return i + off < src.size() ? src[i + off] : '\0'; }
    char peekAt(size_t off) const { return i + off < src.size() ? src[i + off] : '\0'; }

    void adv() {
        if (i < src.size()) {
            if (src[i] == '\n') {
                line++;
                col = 1;
            } else col++;
            i++;
        }
    }

    Token mk(TT t, const std::string& s) {
        Token tk;
        tk.t = t;
        tk.s = s;
        tk.line = line;
        tk.col = col;
        return tk;
    }

    void push(Token tk) { toks.push_back(tk); }

    void skipWsComments() {
        for (;;) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f') {
                adv();
                continue;
            }
            if (c == '/' && peekAt(1) == '/') {
                int sline = line;
                std::string txt;
                while (i < src.size() && src[i] != '\n') {
                    txt += src[i];
                    adv();
                }
                Comment cm;
                cm.text = txt;
                cm.line = sline;
                cm.block = false;
                comments.push_back(cm);
                continue;
            }
            if (c == '/' && peekAt(1) == '*') {
                int sline = line;
                adv();
                adv();
                std::string txt;
                while (i < src.size() && !(src[i] == '*' && peekAt(1) == '/')) {
                    txt += src[i];
                    adv();
                }
                if (i < src.size()) {
                    adv();
                    adv();
                } else {
                    ctx.error(sline, "unterminated block comment");
                }
                Comment cm;
                cm.text = txt;
                cm.line = sline;
                cm.block = true;
                comments.push_back(cm);
                continue;
            }
            break;
        }
    }

    static const std::set<std::string>& keywords() {
        static const std::set<std::string> kw = {
            "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
            "class", "const", "continue", "default", "do", "double", "else", "enum",
            "extends", "final", "finally", "float", "for", "goto", "if", "implements",
            "import", "instanceof", "int", "interface", "long", "native", "new", "package",
            "private", "protected", "public", "return", "short", "static", "strictfp",
            "super", "switch", "synchronized", "this", "throw", "throws", "transient",
            "try", "void", "volatile", "while", "true", "false", "null", "var", "record",
            "yield", "sealed", "permits", "non-sealed"
        };
        return kw;
    }

    std::string readIdent() {
        std::string s;
        while (isIdentPart(peek())) {
            s += peek();
            adv();
        }
        return s;
    }

    std::string readNumber() {
        std::string s;
        if (peek() == '0' && (peekAt(1) == 'x' || peekAt(1) == 'X')) {
            s += peek();
            adv();
            s += peek();
            adv();
            while (isHexDigit(peek()) || peek() == '_') {
                s += peek();
                adv();
            }
            bool hasFraction = false;
            if (peek() == '.' && peekAt(1) != '.') {
                hasFraction = true;
                s += peek();
                adv();
                while (isHexDigit(peek()) || peek() == '_') {
                    s += peek();
                    adv();
                }
            }
            if (peek() == 'p' || peek() == 'P') {
                s += peek();
                adv();
                if (peek() == '+' || peek() == '-') {
                    s += peek();
                    adv();
                }
                size_t exponentStart = i;
                while (isDigit(peek()) || peek() == '_') {
                    s += peek();
                    adv();
                }
                bool hasExponentDigit = false;
                for (size_t k = exponentStart; k < i; ++k)
                    if (isDigit(src[k])) hasExponentDigit = true;
                if (!hasExponentDigit) {
                    ctx.error(line, "hexadecimal floating-point exponent has no digits");
                    return s;
                }
                if (peek() == 'f' || peek() == 'F' || peek() == 'd' || peek() == 'D') {
                    s += peek();
                    adv();
                }
                return normalizeHexFloat(s);
            }
            if (hasFraction) ctx.error(line, "hexadecimal floating-point literal requires a p exponent");
            if (peek() == 'L' || peek() == 'l') {
                s += 'L';
                adv();
            }
            return normalizeBaseInt(s, 16);
        }
        if (peek() == '0' && (peekAt(1) == 'b' || peekAt(1) == 'B')) {
            s += peek();
            adv();
            s += peek();
            adv();
            while (peek() == '0' || peek() == '1' || peek() == '_') {
                s += peek();
                adv();
            }
            if (peek() == 'L' || peek() == 'l') {
                s += 'L';
                adv();
            }
            return normalizeBaseInt(s, 2);
        }
        bool isFloat = false;
        if (peek() == '.') {
            isFloat = true;
            s += peek();
            adv();
            while (isDigit(peek()) || peek() == '_') {
                s += peek();
                adv();
            }
            if (peek() == 'e' || peek() == 'E') {
                s += peek();
                adv();
                if (peek() == '+' || peek() == '-') {
                    s += peek();
                    adv();
                }
                size_t exponentStart = i;
                while (isDigit(peek()) || peek() == '_') {
                    s += peek();
                    adv();
                }
                bool hasExponentDigit = false;
                for (size_t k = exponentStart; k < i; ++k)
                    if (isDigit(src[k])) hasExponentDigit = true;
                if (!hasExponentDigit) ctx.error(line, "floating-point exponent has no digits");
            }
        } else {
            while (isDigit(peek()) || peek() == '_') {
                s += peek();
                adv();
            }
            if (peek() == '.' && peekAt(1) != '.') {
                isFloat = true;
                s += peek();
                adv();
                while (isDigit(peek()) || peek() == '_') {
                    s += peek();
                    adv();
                }
            }
            if (peek() == 'e' || peek() == 'E') {
                isFloat = true;
                s += peek();
                adv();
                if (peek() == '+' || peek() == '-') {
                    s += peek();
                    adv();
                }
                size_t exponentStart = i;
                while (isDigit(peek()) || peek() == '_') {
                    s += peek();
                    adv();
                }
                bool hasExponentDigit = false;
                for (size_t k = exponentStart; k < i; ++k)
                    if (isDigit(src[k])) hasExponentDigit = true;
                if (!hasExponentDigit) ctx.error(line, "floating-point exponent has no digits");
            }
        }
        if (!s.empty() && s[0] == '.') s = "0" + s;
        if (!s.empty() && s.back() == '.') s += "0";
        if (peek() == 'f' || peek() == 'F') {
            isFloat = true;
            s += peek();
            adv();
        } else if (peek() == 'd' || peek() == 'D') {
            isFloat = true;
            s += peek();
            adv();
        } else if (peek() == 'l' || peek() == 'L') {
            s += 'L';
            adv();
        }
        if (!isFloat) s = normalizeInt(s);
        return s;
    }

    bool validNumericSeparators(const std::string& raw) const {
        bool hexadecimal = startsWith(raw, "0x") || startsWith(raw, "0X");
        bool binary = startsWith(raw, "0b") || startsWith(raw, "0B");
        size_t exponent = raw.find(hexadecimal ? 'p' : 'e');
        if (hexadecimal) {
            size_t upper = raw.find('P');
            if (exponent == std::string::npos || (upper != std::string::npos && upper < exponent)) exponent = upper;
        } else {
            size_t upper = raw.find('E');
            if (exponent == std::string::npos || (upper != std::string::npos && upper < exponent)) exponent = upper;
        }
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '_') continue;
            if (i == 0 || i + 1 >= raw.size()) return false;
            bool exponentDigits = exponent != std::string::npos && i > exponent;
            auto digit = [&](char c) {
                if (exponentDigits || !hexadecimal) return isDigit(c);
                if (binary) return c == '0' || c == '1';
                return isHexDigit(c);
            };
            if (!digit(raw[i - 1]) || !digit(raw[i + 1])) return false;
        }
        return true;
    }

    std::string normalizeHexFloat(const std::string& raw) {
        std::string literal;
        for (char c : raw)
            if (c != '_') literal += c;
        bool asFloat = !literal.empty() && (literal.back() == 'f' || literal.back() == 'F');
        if (!literal.empty() && (literal.back() == 'f' || literal.back() == 'F' ||
                                 literal.back() == 'd' || literal.back() == 'D'))
            literal.pop_back();
        errno = 0;
        char* end = nullptr;
        std::ostringstream formatted;
        if (asFloat) {
            float value = std::strtof(literal.c_str(), &end);
            if (end != literal.c_str() + literal.size() || errno == ERANGE || !std::isfinite(value)) {
                ctx.error(line, "hexadecimal floating-point literal is out of range or malformed");
                return raw;
            }
            formatted << std::scientific
                      << std::setprecision(std::numeric_limits<float>::max_digits10 - 1) << value << 'F';
        } else {
            double value = std::strtod(literal.c_str(), &end);
            if (end != literal.c_str() + literal.size() || errno == ERANGE || !std::isfinite(value)) {
                ctx.error(line, "hexadecimal floating-point literal is out of range or malformed");
                return raw;
            }
            formatted << std::scientific
                      << std::setprecision(std::numeric_limits<double>::max_digits10 - 1) << value;
        }
        return formatted.str();
    }

    std::string normalizeBaseInt(const std::string& raw, unsigned int base) {
        std::string body = raw.substr(2);
        bool isLong = !body.empty() && body.back() == 'L';
        if (isLong) body.pop_back();
        std::string digits;
        for (char c : body)
            if (c != '_') digits += c;
        if (digits.empty()) {
            ctx.error(line, "integer literal has no digits");
            return raw;
        }
        std::uint64_t value = 0;
        for (char c : digits) {
            int digit = base == 16 ? hexVal(c) : (c == '0' || c == '1' ? c - '0' : -1);
            if (digit < 0 || static_cast<unsigned int>(digit) >= base) {
                ctx.error(line, "invalid digit in integer literal");
                return raw;
            }
            if (value > (std::numeric_limits<std::uint64_t>::max() -
                         static_cast<unsigned int>(digit)) / base) {
                ctx.error(line, "integer literal is out of range");
                return raw;
            }
            value = value * base + static_cast<unsigned int>(digit);
        }
        if (!isLong) {
            if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                return std::to_string(value);
            if (value <= std::numeric_limits<std::uint32_t>::max()) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "0x%llXU", static_cast<unsigned long long>(value));
                return "unchecked((int)" + std::string(buf) + ")";
            }
            ctx.error(line, "integer literal exceeds the 32-bit Java integer range");
            return raw;
        }
        if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            return std::to_string(value) + "L";
        char buf[40];
        std::snprintf(buf, sizeof(buf), "0x%llXUL", static_cast<unsigned long long>(value));
        return "unchecked((long)" + std::string(buf) + ")";
    }

    std::string normalizeInt(const std::string& raw) {
        std::string suffix;
        std::string body = raw;
        if (!body.empty() && (body.back() == 'L')) {
            suffix = "L";
            body.pop_back();
        }
        std::string digits;
        for (char c : body)
            if (c != '_') digits += c;
        if (digits.size() > 1 && digits[0] == '0') {
            std::uint64_t value = 0;
            for (size_t k = 1; k < digits.size(); k++) {
                if (digits[k] < '0' || digits[k] > '7') {
                    ctx.error(line, "invalid digit in octal integer literal");
                    return digits + suffix;
                }
                unsigned int digit = static_cast<unsigned int>(digits[k] - '0');
                if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 8) {
                    ctx.error(line, "octal integer literal is out of range");
                    return digits + suffix;
                }
                value = value * 8 + digit;
            }
            if (suffix.empty()) {
                if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                    return std::to_string(value);
                if (value <= std::numeric_limits<std::uint32_t>::max()) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "0x%llXU", static_cast<unsigned long long>(value));
                    return "unchecked((int)" + std::string(buf) + ")";
                }
                ctx.error(line, "octal integer literal exceeds the 32-bit Java integer range");
                return digits;
            }
            if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                return std::to_string(value) + suffix;
            char buf[40];
            std::snprintf(buf, sizeof(buf), "0x%llXUL", static_cast<unsigned long long>(value));
            return "unchecked((long)" + std::string(buf) + ")";
        }
        return digits + suffix;
    }

    std::string readString(bool block) {
        int startLine = line;
        std::string s;
        if (block) {
            adv();
            adv();
            adv();
            std::string raw;
            while (i < src.size()) {
                if (src[i] == '"' && peekAt(1) == '"' && peekAt(2) == '"') break;
                raw += src[i];
                adv();
            }
            if (i < src.size()) {
                adv();
                adv();
                adv();
            } else {
                ctx.error(startLine, "unterminated text block");
            }
            if (startsWith(raw, "\r\n")) raw.erase(0, 2);
            else if (!raw.empty() && (raw[0] == '\r' || raw[0] == '\n')) raw.erase(0, 1);
            else ctx.error(startLine, "text block opening delimiter must be followed by a line terminator");
            raw = replaceAllS(raw, "\r\n", "\n");
            raw = replaceAllS(raw, "\r", "\n");
            std::vector<std::string> lines = splitStr(raw, '\n');
            size_t incidental = std::string::npos;
            for (const std::string& ln : lines) {
                size_t k = 0;
                while (k < ln.size() && (ln[k] == ' ' || ln[k] == '\t')) k++;
                if (k == ln.size() && &ln != &lines.back()) continue;
                incidental = std::min(incidental, k);
            }
            if (incidental == std::string::npos) incidental = 0;
            std::string content;
            for (size_t k = 0; k < lines.size(); k++) {
                std::string ln = lines[k];
                size_t remove = 0;
                while (remove < incidental && remove < ln.size() &&
                       (ln[remove] == ' ' || ln[remove] == '\t')) remove++;
                ln.erase(0, remove);
                while (!ln.empty() && (ln.back() == ' ' || ln.back() == '\t')) ln.pop_back();
                if (k) content += "\n";
                content += ln;
            }
            return decodeJavaEscapes(content);
        }
        adv();
        std::string raw;
        bool closed = false;
        while (i < src.size() && src[i] != '"' && src[i] != '\n' && src[i] != '\r') {
            if (src[i] == '\\') {
                raw += src[i];
                adv();
                if (i < src.size() && src[i] != '\n' && src[i] != '\r') {
                    raw += src[i];
                    adv();
                }
            } else {
                raw += src[i];
                adv();
            }
        }
        if (i < src.size() && src[i] == '"') {
            adv();
            closed = true;
        }
        if (!closed) ctx.error(startLine, "unterminated string literal");
        return decodeJavaEscapes(raw);
    }

    std::string readChar() {
        int startLine = line;
        adv();
        std::string raw;
        bool closed = false;
        while (i < src.size() && src[i] != '\'' && src[i] != '\n' && src[i] != '\r') {
            if (src[i] == '\\') {
                raw += src[i];
                adv();
                if (i < src.size() && src[i] != '\n' && src[i] != '\r') {
                    raw += src[i];
                    adv();
                }
            } else {
                raw += src[i];
                adv();
            }
        }
        if (i < src.size() && src[i] == '\'') {
            adv();
            closed = true;
        }
        if (!closed) ctx.error(startLine, "unterminated character literal");
        std::string value = decodeJavaEscapes(raw);
        if (utf16UnitCount(value) != 1)
            ctx.error(startLine, "character literal must contain one UTF-16 code unit");
        return value;
    }

    bool scanTripleQuote() const {
        return peek() == '"' && peekAt(1) == '"' && peekAt(2) == '"';
    }

    void run() {
        for (;;) {
            skipWsComments();
            if (i >= src.size()) break;
            char c = peek();
            if (isIdentStart(c)) {
                int sl2 = line;
                std::string id = readIdent();
                if (id == "non" && peek() == '-' && i + 7 <= src.size() &&
                    src.compare(i + 1, 6, "sealed") == 0 && !isIdentPart(peekAt(7))) {
                    for (int k = 0; k < 7; k++) adv();
                    id = "non-sealed";
                }
                TT t = TT::Id;
                if (keywords().count(id)) {
                    if (id == "true" || id == "false") t = TT::Bool;
                    else if (id == "null") t = TT::Null;
                    else t = TT::Kw;
                }
                Token tk = mk(t, id);
                tk.line = sl2;
                push(tk);
                continue;
            }
            if (c == '.' && peekAt(1) == '.' && peekAt(2) == '.') {
                Token tk = mk(TT::Punct, "...");
                tk.line = line;
                adv();
                adv();
                adv();
                push(tk);
                continue;
            }
            if (isDigit(c) || (c == '.' && isDigit(peekAt(1)))) {
                int sl2 = line;
                size_t numberStart = i;
                std::string num = readNumber();
                std::string rawNumber = src.substr(numberStart, i - numberStart);
                if (!validNumericSeparators(rawNumber))
                    ctx.error(sl2, "invalid placement of underscore in numeric literal");
                bool isHexOrBinary = startsWith(rawNumber, "0x") || startsWith(rawNumber, "0X") ||
                                     startsWith(rawNumber, "0b") || startsWith(rawNumber, "0B");
                bool isFlt = isHexOrBinary
                    ? (rawNumber.find('p') != std::string::npos || rawNumber.find('P') != std::string::npos)
                    : (rawNumber.find('.') != std::string::npos || rawNumber.find('e') != std::string::npos ||
                       rawNumber.find('E') != std::string::npos || endsWith(rawNumber, "f") ||
                       endsWith(rawNumber, "F") || endsWith(rawNumber, "d") || endsWith(rawNumber, "D"));
                Token tk = mk(isFlt ? TT::Flt : TT::Int, num);
                tk.line = sl2;
                push(tk);
                continue;
            }
            if (c == '"') {
                int sl2 = line;
                if (scanTripleQuote()) {
                    std::string val = readString(true);
                    Token tk = mk(TT::TBlk, val);
                    tk.line = sl2;
                    push(tk);
                } else {
                    std::string val = readString(false);
                    Token tk = mk(TT::Str, val);
                    tk.line = sl2;
                    push(tk);
                }
                continue;
            }
            if (c == '\'') {
                int sl2 = line;
                std::string val = readChar();
                Token tk = mk(TT::Chr, val);
                tk.line = sl2;
                push(tk);
                continue;
            }
            static const char* ops3[] = {">>>=", nullptr};
            static const char* ops2[] = {
                ">>>", "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=",
                "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "::", "->", "<<", ">>", nullptr};
            std::string matched;
            for (int k = 0; ops3[k]; k++) {
                std::string o = ops3[k];
                if (startsWith(src.substr(i), o)) {
                    matched = o;
                    break;
                }
            }
            if (matched.empty()) {
                for (int k = 0; ops2[k]; k++) {
                    std::string o = ops2[k];
                    if (startsWith(src.substr(i), o)) {
                        matched = o;
                        break;
                    }
                }
            }
            if (matched.empty()) matched = std::string(1, c);
            int sl2 = line;
            for (size_t k = 0; k < matched.size(); k++) adv();
            Token tk = mk(TT::Punct, matched);
            tk.line = sl2;
            push(tk);
        }
        Token end;
        end.t = TT::End;
        end.s = "";
        end.line = line;
        end.col = col;
        push(end);
    }
};
}
