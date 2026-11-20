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
struct Parser {
    std::vector<Token> ts;
    size_t p = 0;
    Ctx& ctx;

    explicit Parser(std::vector<Token> toks, Ctx& c) : ts(std::move(toks)), ctx(c) {}

    const Token& cur(size_t off = 0) const {
        size_t k = p + off;
        return k < ts.size() ? ts[k] : ts.back();
    }
    TT curT(size_t off = 0) const { return cur(off).t; }
    std::string curS(size_t off = 0) const { return cur(off).s; }
    int curLine() const { return cur().line; }

    bool isKw(const std::string& k, size_t off = 0) const { return curT(off) == TT::Kw && curS(off) == k; }
    bool isP(const std::string& s, size_t off = 0) const { return curT(off) == TT::Punct && curS(off) == s; }
    bool isId(const std::string& s, size_t off = 0) const { return curT(off) == TT::Id && curS(off) == s; }
    bool isIdAny(size_t off = 0) const { return curT(off) == TT::Id; }

    void adv() {
        if (p + 1 < ts.size()) p++;
        else p = ts.size() - 1;
    }

    bool eatKw(const std::string& k) {
        if (isKw(k)) {
            adv();
            return true;
        }
        return false;
    }
    bool eatP(const std::string& s) {
        if (isP(s)) {
            adv();
            return true;
        }
        return false;
    }
    bool eatId(const std::string& s) {
        if (isId(s)) {
            adv();
            return true;
        }
        return false;
    }
    bool expectP(const std::string& s) {
        if (eatP(s)) return true;
        ctx.error(curLine(), "expected '" + s + "' but found '" + curS() + "'");
        return false;
    }
    std::string expectId() {
        if (isIdAny()) {
            std::string s = curS();
            adv();
            return s;
        }
        ctx.error(curLine(), "expected identifier but found '" + curS() + "'");
        return "_";
    }

    void syncResync() {
        int depth = 0;
        for (;;) {
            TT t = curT();
            const std::string& s = curS();
            if (t == TT::End) return;
            if (depth == 0) {
                if (s == ";") {
                    adv();
                    return;
                }
                if (s == "}") {
                    adv();
                    return;
                }
            }
            if (s == "{" || s == "(" || s == "[") depth++;
            if (s == "}" || s == ")" || s == "]") {
                if (depth > 0) depth--;
                else return;
            }
            adv();
        }
    }

    static const std::set<std::string>& modKws() {
        static const std::set<std::string> m = {
            "public", "private", "protected", "static", "final", "abstract", "native",
            "synchronized", "transient", "volatile", "strictfp", "const", "default"};
        return m;
    }
    static bool isAnnStart(size_t off, const Parser& self) {
        return self.curT(off) == TT::Punct && self.curS(off) == "@";
    }

    Ann parseAnn() {
        Ann a;
        a.line = curLine();
        adv();
        a.name = expectId();
        while (isP(".") && isIdAny(1)) {
            adv();
            a.name += "." + expectId();
        }
        if (isP("(")) {
            a.hasArgs = true;
            adv();
            while (!isP(")") && curT() != TT::End) {
                if (isIdAny() && isP("=", 1)) {
                    std::string nm = expectId();
                    adv();
                    a.args.push_back({nm, parseExpr()});
                } else {
                    a.pargs.push_back(parseExpr());
                }
                if (!eatP(",")) break;
            }
            expectP(")");
        }
        return a;
    }

    bool parseMods(std::vector<std::string>& mods, std::vector<Ann>& anns) {
        bool any = false;
        for (;;) {
            if (curT() == TT::Punct && curS() == "@" && !isKw("interface", 1)) {
                anns.push_back(parseAnn());
                any = true;
                continue;
            }
            if (curT() == TT::Kw && modKws().count(curS()) && !isKw("default")) {
                mods.push_back(curS());
                adv();
                any = true;
                continue;
            }
            if (isKw("default")) {
                mods.push_back("default");
                adv();
                any = true;
                continue;
            }
            if ((curT() == TT::Kw || curT() == TT::Id) &&
                (curS() == "sealed" || curS() == "non-sealed")) {
                adv();
                any = true;
                continue;
            }
            break;
        }
        return any;
    }

    bool eatGenericClose() {
        if (isP(">")) {
            adv();
            return true;
        }
        if (isP(">>")) {
            ts[p].s = ">";
            return true;
        }
        if (isP(">>>")) {
            ts[p].s = ">>";
            return true;
        }
        return false;
    }

    std::vector<TP> parseTypeParams() {
        std::vector<TP> out;
        if (!eatP("<")) return out;
        for (;;) {
            TP tp;
            if (curT() == TT::Id) tp.name = expectId();
            else tp.name = "_";
            if (eatKw("extends")) {
                tp.hasBound = true;
                tp.bound = parseTypeRef(true, false);
                while (eatP("&")) {
                    TypeRef extra = parseTypeRef(true, false);
                    if (tp.bound.name.empty()) tp.bound = extra;
                }
            }
            out.push_back(tp);
            if (!eatP(",")) break;
        }
        if (!eatGenericClose()) expectP(">");
        return out;
    }

    TypeRef parseTypeRef(bool allowWild = true, bool allowVar = true) {
        TypeRef t;
        if (allowWild && isP("?")) {
            t.wildcard = true;
            adv();
            if (eatKw("extends") || eatKw("super")) {
                TypeRef b = parseTypeRef(false, false);
                t.name = b.name;
                t.args = b.args;
                t.dims = b.dims;
            }
        } else {
            if (isKw("var")) {
                t.isVar = true;
                t.name = "var";
                adv();
            } else if (curT() == TT::Kw && primitiveTypes().count(curS())) {
                t.name = curS();
                adv();
            } else if (isIdAny()) {
                t.name = expectId();
                while (isP(".") && (isIdAny(1) || (isKw("class", 1)))) {
                    if (!isIdAny(1)) break;
                    adv();
                    t.name += "." + expectId();
                }
                if (isP("<")) {
                    adv();
                    t.diamond = false;
                    for (;;) {
                        if (isP(">") && t.args.empty()) {
                            t.diamond = true;
                            break;
                        }
                        t.args.push_back(parseTypeRef(true, true));
                        if (!eatP(",")) break;
                    }
                    if (!eatGenericClose()) expectP(">");
                }
            } else {
                ctx.error(curLine(), "expected type but found '" + curS() + "'");
                t.name = "object";
            }
        }
        while (isP("[") && isP("]", 1)) {
            adv();
            adv();
            t.dims++;
        }
        (void)allowVar;
        return t;
    }

    std::unique_ptr<Unit> parseUnit() {
        auto u = std::make_unique<Unit>();
        u->line = curLine();
        if (curT() == TT::Kw && curS() == "package") {
            adv();
            u->package = expectId();
            while (eatP(".")) u->package += "." + expectId();
            expectP(";");
        }
        while (curT() == TT::Kw && curS() == "import") {
            adv();
            bool stat = eatKw("static");
            std::string path = expectId();
            bool wild = false;
            while (isP(".")) {
                adv();
                if (eatP("*")) {
                    wild = true;
                    break;
                }
                if (isKw("class")) break;
                path += "." + expectId();
            }
            expectP(";");
            u->imports.push_back({wild ? path + ".*" : path, stat});
        }
        while (curT() != TT::End) {
            if (isP(";") || isP("}")) {
                adv();
                continue;
            }
            size_t before = p;
            auto d = parseTypeDecl();
            if (d) u->types.push_back(std::move(d));
            if (p == before) adv();
        }
        return u;
    }

    bool atTypeStart() const {
        if (curT() == TT::Kw && (curS() == "class" || curS() == "interface" || curS() == "enum" || curS() == "record"))
            return true;
        if (curT() == TT::Punct && curS() == "@" && isKw("interface", 1)) return true;
        return false;
    }

    std::unique_ptr<Decl> parseTypeDecl() {
        std::vector<std::string> mods;
        std::vector<Ann> anns;
        parseMods(mods, anns);
        int line = curLine();
        std::unique_ptr<Decl> d = std::make_unique<Decl>();
        d->mods = mods;
        d->anns = std::move(anns);
        d->line = line;
        if (curT() == TT::Punct && curS() == "@" && isKw("interface", 1)) {
            adv();
            adv();
            d->dk = 4;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            parseInheritance(*d, true);
            parseTypeBody(*d, true, false, nullptr);
            return d;
        }
        if (isKw("interface")) {
            adv();
            d->dk = 1;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            parseInheritance(*d, true);
            parseTypeBody(*d, true, false, nullptr);
            return d;
        }
        if (isKw("class")) {
            adv();
            d->dk = 0;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            parseInheritance(*d, false);
            parseTypeBody(*d, false, false, nullptr);
            return d;
        }
        if (isKw("enum")) {
            adv();
            d->dk = 2;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            if (eatKw("implements")) {
                d->impls.push_back(parseTypeRef());
                while (eatP(",")) d->impls.push_back(parseTypeRef());
            }
            parseEnumBody(*d);
            return d;
        }
        if (isKw("record")) {
            adv();
            d->dk = 3;
            d->name = expectId();
            if (isP("<")) d->tparams = parseTypeParams();
            auto comps = parseParams(nullptr);
            if (eatKw("implements")) {
                d->impls.push_back(parseTypeRef());
                while (eatP(",")) d->impls.push_back(parseTypeRef());
            }
            parseTypeBody(*d, false, true, &comps);
            return d;
        }
        ctx.error(line, "expected type declaration but found '" + curS() + "'");
        syncResync();
        return nullptr;
    }

    void parseInheritance(Decl& d, bool iface) {
        if (eatKw("extends")) {
            d.extends = parseTypeRef();
            if (iface) {
                while (eatP(",")) {
                    TypeRef t = parseTypeRef();
                    d.impls.push_back(t);
                }
            }
        }
        if (eatKw("implements")) {
            d.impls.push_back(parseTypeRef());
            while (eatP(",")) d.impls.push_back(parseTypeRef());
        }
        if (eatKw("permits")) {
            parseTypeRef();
            while (eatP(",")) parseTypeRef();
        }
    }

    void parseTypeBody(Decl& d, bool isIface, bool isRecord, std::vector<std::pair<TypeRef, std::string>>* comps) {
        expectP("{");
        if (isRecord && comps) {
            Member m;
            m.mk = 2;
            m.ty = TypeRef();
            m.params = *comps;
            m.mods.push_back("__recordctor");
            m.body = std::make_unique<Stmt>();
            m.body->k = SK::Block;
            m.body->line = d.line;
            d.members.push_back(std::move(m));
        }
        while (!isP("}") && curT() != TT::End) {
            if (isP(";")) {
                adv();
                continue;
            }
            size_t before = p;
            parseMember(d, isIface, isRecord);
            if (p == before) adv();
        }
        expectP("}");
    }

    void parseEnumBody(Decl& d) {
        expectP("{");
        while (curT() == TT::Id && (isP(",", 1) || isP("(", 1) || isP("{", 1) || isP(";", 1) || isP("}", 1))) {
            EnumConst ec;
            ec.line = curLine();
            ec.name = expectId();
            if (isP("(")) {
                adv();
                while (!isP(")") && curT() != TT::End) {
                    ec.args.push_back(parseExpr());
                    if (!eatP(",")) break;
                }
                expectP(")");
            }
            if (isP("{")) {
                auto body = std::make_unique<Decl>();
                body->dk = 0;
                body->name = d.name + "_" + ec.name;
                body->mods.push_back("private");
                body->mods.push_back("final");
                std::vector<std::pair<TypeRef, std::string>> none;
                parseTypeBody(*body, false, false, &none);
                ec.body = std::move(body);
            }
            d.consts.push_back(std::move(ec));
            if (!eatP(",")) break;
        }
        if (eatP(";")) {
            while (!isP("}") && curT() != TT::End) {
                if (isP(";")) {
                    adv();
                    continue;
                }
                size_t before = p;
                parseMember(d, false, false);
                if (p == before) adv();
            }
        }
        expectP("}");
    }

    void parseMember(Decl& d, bool isIface, bool isRecord) {
        std::vector<std::string> mods;
        std::vector<Ann> anns;
        int line = curLine();
        parseMods(mods, anns);
        if (isP("{")) {
            Member m;
            m.mk = 3;
            m.mods = mods;
            m.anns = std::move(anns);
            m.line = line;
            if (std::find(m.mods.begin(), m.mods.end(), "static") != m.mods.end()) {
                m.isStaticInit = true;
                m.mods.erase(std::remove(m.mods.begin(), m.mods.end(), "static"), m.mods.end());
            }
            m.body = parseBlock();
            d.members.push_back(std::move(m));
            return;
        }
        if (atTypeStart()) {
            auto nd = parseTypeDecl();
            if (nd) {
                nd->mods.insert(nd->mods.begin(), mods.begin(), mods.end());
                nd->anns.insert(nd->anns.begin(), std::make_move_iterator(anns.begin()),
                                std::make_move_iterator(anns.end()));
                Member m;
                m.mk = 4;
                m.mods = mods;
                m.line = line;
                m.nested = std::move(nd);
                d.members.push_back(std::move(m));
            }
            return;
        }
        bool ctorGeneric = isP("<") && isId(d.name, 1);
        if (getenv("JS_DEBUG")) fprintf(stderr, "[member] d=%s cur='%s' next='%s' idmatch=%d paren=%d\n", d.name.c_str(), curS().c_str(), curS(1).c_str(), (int)isId(d.name), (int)isP("(", 1));
        if (isRecord && isId(d.name) && isP("{", 1)) {
            Member m;
            m.mk = 2;
            m.mods = mods;
            m.mods.push_back("__recordctor");
            m.anns = std::move(anns);
            m.line = line;
            m.compactCtor = true;
            m.name = d.name;
            m.ty.name = d.name;
            adv();
            m.body = parseBlock();
            d.members.push_back(std::move(m));
            return;
        }
        if ((isId(d.name) && isP("(", 1)) || ctorGeneric) {
            Member m;
            m.mk = 2;
            m.mods = mods;
            m.anns = std::move(anns);
            m.line = line;
            m.name = d.name;
            m.ty.name = d.name;
            if (ctorGeneric) m.tparams = parseTypeParams();
            else adv();
            bool va = false;
            m.params = parseParams(&va);
            m.varargs = va;
            parseThrows(m);
            if (isP("{")) {
                m.body = parseBlock();
            } else {
                expectP(";");
            }
            d.members.push_back(std::move(m));
            return;
        }
        if (isIface && curT() == TT::Kw && curS() == "void" && isIdAny(1) && isP("(", 2)) {
            TypeRef rt;
            rt.name = "void";
            Member m;
            m.mk = 1;
            m.mods = mods;
            m.anns = std::move(anns);
            m.ty = rt;
            m.line = line;
            adv();
            m.name = expectId();
            bool va = false;
            m.params = parseParams(&va);
            m.varargs = va;
            parseThrows(m);
            if (isP("{")) m.body = parseBlock();
            else expectP(";");
            d.members.push_back(std::move(m));
            return;
        }
        Member m;
        m.mods = mods;
        m.anns = std::move(anns);
        m.line = line;
        if (isP("<")) m.tparams = parseTypeParams();
        m.ty = parseTypeRef(true, true);
        bool isMethod = false;
        std::string name;
        if (curT() == TT::Kw && primitiveTypes().count(curS()) && isIdAny(1)) {
            name = curS(1);
            isMethod = true;
        } else if (curT() == TT::Kw && curS() == "void" && isIdAny(1)) {
            name = curS(1);
            isMethod = true;
        } else if (isIdAny()) {
            name = curS();
            if (isP("(", 1)) isMethod = true;
            else if (isP("<", 1)) isMethod = true;
            else adv();
        } else {
            ctx.error(line, "expected member but found '" + curS() + "'");
            syncResync();
            return;
        }
        if (isMethod) {
            adv();
            m.mk = 1;
            m.name = name;
            if (isP("<")) m.tparams = parseTypeParams();
            bool va = false;
            m.params = parseParams(&va);
            m.varargs = va;
            parseThrows(m);
            if (isP("{")) {
                m.body = parseBlock();
            } else if (isKw("default")) {
                adv();
                auto blk = std::make_unique<Stmt>();
                blk->k = SK::Block;
                blk->line = curLine();
                auto ret = std::make_unique<Stmt>();
                ret->k = SK::Return;
                ret->line = curLine();
                ret->e = parseExpr();
                blk->block.push_back(std::move(ret));
                m.body = std::move(blk);
                expectP(";");
            } else {
                expectP(";");
            }
            d.members.push_back(std::move(m));
            return;
        }
        m.mk = 0;
        for (;;) {
            m.names.push_back(name);
            int extra = 0;
            while (isP("[") && isP("]", 1)) {
                adv();
                adv();
                extra++;
            }
            m.fdims.push_back(extra);
            if (eatP("=")) {
                m.finits.push_back(parseVarInit());
            } else {
                m.finits.push_back(nullptr);
            }
            if (eatP(",")) {
                if (curT() == TT::Kw && primitiveTypes().count(curS()) && isIdAny(1)) {
                    name = curS(1);
                    adv();
                    adv();
                } else if (curT() == TT::Kw && curS() == "void" && isIdAny(1)) {
                    name = curS(1);
                    adv();
                    adv();
                } else {
                    name = expectId();
                }
                continue;
            }
            break;
        }
        expectP(";");
        d.members.push_back(std::move(m));
        (void)isRecord;
    }

    void parseThrows(Member& m) {
        if (eatKw("throws")) {
            m.throws.push_back(parseTypeRef());
            while (eatP(",")) m.throws.push_back(parseTypeRef());
        }
    }

    bool tryLocalVarHeader(TypeRef* outTy, std::string* outName);
    std::unique_ptr<Expr> parseAssign();
    std::vector<std::pair<TypeRef, std::string>> parseParams(bool* varargs);
    std::unique_ptr<Expr> parseVarInit();
    std::unique_ptr<Stmt> parseBlock();
    std::vector<std::unique_ptr<Stmt>> parseStmtListUntil(const std::string& closer, bool consume);
    std::unique_ptr<Stmt> parseStmt();
    std::vector<std::unique_ptr<Stmt>> parseCaseBody();
    std::unique_ptr<Stmt> parseLocalVar(bool allowFinal);
    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseExprNoLambda();
    std::unique_ptr<Expr> parseTernary();
    std::unique_ptr<Expr> parseBinary(int minPrec);
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfixExpr();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parseNew();
    std::unique_ptr<Expr> parseLambdaOrExpr();
    std::unique_ptr<Expr> parseParOrLambda();
    bool looksLikeLambda(size_t off) const;
    int probeParenType(size_t off, bool* primOut) const;
    bool parseGenericTypeArgs(std::vector<TypeRef>& out);
};
std::vector<std::pair<TypeRef, std::string>> Parser::parseParams(bool* varargs) {
    std::vector<std::pair<TypeRef, std::string>> out;
    if (getenv("JS_DEBUG")) fprintf(stderr, "[params] enter cur='%s'\n", curS().c_str());
    expectP("(");
    if (!isP(")")) {
        for (;;) {
            while (isKw("final")) adv();
            while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
            TypeRef t = parseTypeRef(true, false);
            if (isP("...")) {
                adv();
                if (varargs) *varargs = true;
            }
            std::string nm = expectId();
            int extra = 0;
            while (isP("[") && isP("]", 1)) {
                adv();
                adv();
                extra++;
            }
            t.dims += extra;
            out.push_back({t, nm});
            if (!eatP(",")) break;
        }
    }
    expectP(")");
    return out;
}

std::unique_ptr<Expr> Parser::parseVarInit() {
    if (isP("{")) {
        auto e = std::make_unique<Expr>();
        e->k = EK::NewArr;
        e->implArr = true;
        e->line = curLine();
        adv();
        while (!isP("}") && curT() != TT::End) {
            e->args.push_back(parseVarInit());
            if (!eatP(",")) break;
        }
        expectP("}");
        return e;
    }
    return parseExpr();
}

std::unique_ptr<Stmt> Parser::parseBlock() {
    auto s = std::make_unique<Stmt>();
    s->k = SK::Block;
    s->line = curLine();
    expectP("{");
    s->block = parseStmtListUntil("}", true);
    return s;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseStmtListUntil(const std::string& closer, bool consume) {
    std::vector<std::unique_ptr<Stmt>> out;
    while (!isP(closer) && curT() != TT::End) {
        if (isP(";")) {
            adv();
            continue;
        }
        auto st = parseStmt();
        if (st) out.push_back(std::move(st));
        else break;
    }
    if (consume) expectP(closer);
    return out;
}

bool Parser::tryLocalVarHeader(TypeRef* outTy, std::string* outName) {
    size_t save = p;
    while (isKw("final")) adv();
    while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
    bool ok = true;
    TypeRef t;
    if (curT() == TT::Kw && primitiveTypes().count(curS())) {
        t = parseTypeRef(false, false);
    } else if (isIdAny() || isKw("var")) {
        t = parseTypeRef(true, true);
    } else {
        ok = false;
    }
    if (ok) {
        if (isIdAny()) {
            std::string nm = curS();
            size_t save2 = p;
            adv();
            if (isP("=") || isP(";") || isP(",") || isP(":")) {
                if (outTy) *outTy = t;
                if (outName) *outName = nm;
                p = save2;
                return true;
            }
            p = save2;
        }
    }
    p = save;
    return false;
}

std::unique_ptr<Stmt> Parser::parseLocalVar(bool allowFinal) {
    auto s = std::make_unique<Stmt>();
    s->k = SK::LocalVar;
    s->line = curLine();
    (void)allowFinal;
    while (isKw("final")) adv();
    while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
    s->ty = parseTypeRef(true, true);
    for (;;) {
        std::string nm = isIdAny() ? expectId() : expectId();
        int extra = 0;
        while (isP("[") && isP("]", 1)) {
            adv();
            adv();
            extra++;
        }
        s->names.push_back(nm);
        s->dims.push_back(extra);
        if (eatP("=")) {
            s->inits.push_back(parseVarInit());
        } else {
            s->inits.push_back(nullptr);
        }
        if (!eatP(",")) break;
    }
    expectP(";");
    return s;
}

std::unique_ptr<Stmt> Parser::parseStmt() {
    int line = curLine();
    while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
    auto typeAhead = [&](size_t off) {
        return isKw("class", off) || isKw("interface", off) || isKw("enum", off) ||
               isKw("record", off) || (isP("@", off) && isKw("interface", off + 1));
    };
    bool localTypeStart = typeAhead(0) ||
                          ((isKw("final") || isKw("abstract")) && typeAhead(1));
    if (localTypeStart) {
        auto d = parseTypeDecl();
        auto s = std::make_unique<Stmt>();
        s->line = line;
        if (d) {
            s->k = SK::LocalClass;
            s->lclass = std::move(d);
        } else {
            s->k = SK::Empty;
        }
        return s;
    }
    if (isP("{")) return parseBlock();
    if (isP(";")) {
        auto s = std::make_unique<Stmt>();
        s->k = SK::Empty;
        s->line = line;
        adv();
        return s;
    }
    if (isIdAny() && isP(":", 1) && curS() != "default") {
        auto s = std::make_unique<Stmt>();
        s->k = SK::Label;
        s->line = line;
        s->label = expectId();
        adv();
        s->body = parseStmt();
        return s;
    }
    if (isKw("if")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::If;
        s->line = line;
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        s->body = parseStmt();
        if (eatKw("else")) s->body2 = parseStmt();
        return s;
    }
    if (isKw("while")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::While;
        s->line = line;
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        s->body = parseStmt();
        return s;
    }
    if (isKw("do")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::DoWhile;
        s->line = line;
        s->body = parseStmt();
        if (!eatKw("while")) ctx.error(curLine(), "expected 'while' after do-block");
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        expectP(";");
        return s;
    }
    if (isKw("for")) {
        adv();
        expectP("(");
        bool enh = false;
        {
            size_t save = p;
            while (isKw("final")) adv();
            if (isIdAny() || isKw("var") || (curT() == TT::Kw && primitiveTypes().count(curS()))) {
                TypeRef t = parseTypeRef(true, true);
                if ((isIdAny() || isKw("yield")) && isP(":", 1)) enh = true;
                (void)t;
            }
            p = save;
        }
        auto s = std::make_unique<Stmt>();
        s->line = line;
        if (enh) {
            s->k = SK::Foreach;
            while (isKw("final")) adv();
            s->ty = parseTypeRef(true, true);
            s->names.push_back(isIdAny() ? expectId() : "_");
            expectP(":");
            s->e = parseExpr();
            expectP(")");
            s->body = parseStmt();
            return s;
        }
        s->k = SK::For;
        if (!isP(";")) {
            size_t save = p;
            TypeRef vty;
            std::string vnm;
            bool decl = tryLocalVarHeader(&vty, &vnm);
            if (decl) {
                s->ty = vty;
                for (;;) {
                    std::string nm = expectId();
                    int extra = 0;
                    while (isP("[") && isP("]", 1)) {
                        adv();
                        adv();
                        extra++;
                    }
                    s->names.push_back(nm);
                    s->dims.push_back(extra);
                    if (eatP("=")) s->inits.push_back(parseVarInit());
                    else s->inits.push_back(nullptr);
                    if (!eatP(",")) break;
                }
            } else {
                p = save;
                for (;;) {
                    s->initExprs.push_back(parseExpr());
                    if (!eatP(",")) break;
                }
            }
        }
        expectP(";");
        if (!isP(";")) s->e = parseExpr();
        expectP(";");
        if (!isP(")")) {
            for (;;) {
                s->updateExprs.push_back(parseExpr());
                if (!eatP(",")) break;
            }
        }
        expectP(")");
        s->body = parseStmt();
        return s;
    }
    if (isKw("return")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Return;
        s->line = line;
        if (!isP(";")) s->e = parseExpr();
        expectP(";");
        return s;
    }
    if (isKw("yield")) {
        if (isP(";", 1) || isIdAny(1) || curT(1) == TT::Int || curT(1) == TT::Str ||
            curT(1) == TT::Chr || curT(1) == TT::Flt || isP("(", 1)) {
            adv();
            auto s = std::make_unique<Stmt>();
            s->k = SK::Return;
            s->isYield = true;
            s->line = line;
            if (!isP(";")) s->e = parseExpr();
            expectP(";");
            return s;
        }
    }
    if (isKw("throw")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Throw;
        s->line = line;
        s->e = parseExpr();
        expectP(";");
        return s;
    }
    if (isKw("break")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Break;
        s->line = line;
        if (isIdAny()) s->label = expectId();
        expectP(";");
        return s;
    }
    if (isKw("continue")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Continue;
        s->line = line;
        if (isIdAny()) s->label = expectId();
        expectP(";");
        return s;
    }
    if (isKw("assert")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Assert;
        s->line = line;
        s->e = parseExpr();
        if (eatP(":")) s->e2 = parseExpr();
        expectP(";");
        return s;
    }
    if (isKw("synchronized")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Sync;
        s->line = line;
        expectP("(");
        s->e = parseExpr();
        expectP(")");
        s->body = parseStmt();
        return s;
    }
    if (isKw("switch")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Switch;
        s->line = line;
        expectP("(");
        s->subject = parseExpr();
        expectP(")");
        expectP("{");
        bool anyArrow = false;
        while (!isP("}") && curT() != TT::End) {
            CaseArm arm;
            arm.line = curLine();
            if (eatP(";")) continue;
            if (isKw("default")) {
                adv();
                arm.isDefault = true;
            } else if (eatKw("case")) {
                for (;;) {
                    if (curT() == TT::Null) {
                        adv();
                        arm.nullLabel = true;
                    } else if (isP("(")) {
                        adv();
                        arm.labels.push_back(parseExpr());
                        expectP(")");
                    } else {
                        size_t save = p;
                        bool patOk = false;
                        TypeRef pt;
                        std::string pn;
                        if ((isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS()))) &&
                            (isIdAny(1) && curS(1) != "when")) {
                            pt = parseTypeRef(false, false);
                            if (isIdAny() && curS() != "when") {
                                pn = expectId();
                                patOk = true;
                            }
                        }
                        if (patOk) {
                            arm.patLabels.push_back(pt);
                            arm.patName = pn;
                        } else {
                            p = save;
                            arm.labels.push_back(parseExprNoLambda());
                        }
                    }
                    if (eatP(",")) continue;
                    break;
                }
            } else {
                ctx.error(curLine(), "expected 'case' or 'default' in switch");
                break;
            }
            if ((isKw("when") || isId("when"))) {
                adv();
                arm.guard = parseExpr();
            }
            if (eatP("->")) {
                arm.arrow = true;
                anyArrow = true;
                if (isP("{")) {
                    arm.isBlockVal = true;
                    adv();
                    arm.body = parseStmtListUntil("}", true);
                } else if (isKw("throw")) {
                    adv();
                    auto th = std::make_unique<Stmt>();
                    th->k = SK::Throw;
                    th->line = curLine();
                    th->e = parseExpr();
                    expectP(";");
                    arm.body.push_back(std::move(th));
                } else {
                    arm.value = parseExpr();
                    expectP(";");
                }
            } else {
                expectP(":");
                arm.body = parseCaseBody();
            }
            s->arms.push_back(std::move(arm));
        }
        expectP("}");
        s->arrowSw = anyArrow;
        return s;
    }
    if (isKw("try")) {
        adv();
        auto s = std::make_unique<Stmt>();
        s->k = SK::Try;
        s->line = line;
        if (isP("(")) {
            adv();
            while (!isP(")") && curT() != TT::End) {
                while (isKw("final")) adv();
                while (curT() == TT::Punct && curS() == "@") (void)parseAnn();
                TypeRef rt = parseTypeRef(true, true);
                std::string rn = isIdAny() ? expectId() : "_";
                expectP("=");
                s->resources.push_back({rt, rn});
                s->resInits.push_back(parseExpr());
                if (!eatP(";")) break;
            }
            expectP(")");
        }
        s->body = parseBlock();
        while (isKw("catch")) {
            adv();
            Stmt::Catch c;
            expectP("(");
            for (;;) {
                c.types.push_back(parseTypeRef(true, false));
                if (!eatP("|")) break;
            }
            while (isKw("final")) adv();
            c.var = expectId();
            expectP(")");
            c.body = parseBlock();
            s->catches.push_back(std::move(c));
        }
        if (eatKw("finally")) s->fin = parseBlock();
        return s;
    }
    {
        TypeRef vty;
        std::string vnm;
        size_t saveLV = p;
        if (tryLocalVarHeader(&vty, &vnm)) {
            auto s = std::make_unique<Stmt>();
            s->k = SK::LocalVar;
            s->line = line;
            s->ty = vty;
            for (;;) {
                std::string nm = expectId();
                int extra = 0;
                while (isP("[") && isP("]", 1)) {
                    adv();
                    adv();
                    extra++;
                }
                s->names.push_back(nm);
                s->dims.push_back(extra);
                if (eatP("=")) s->inits.push_back(parseVarInit());
                else s->inits.push_back(nullptr);
                if (!eatP(",")) break;
            }
            expectP(";");
            return s;
        }
        p = saveLV;
    }
    auto s = std::make_unique<Stmt>();
    s->k = SK::ExprStmt;
    s->line = line;
    s->e = parseExpr();
    expectP(";");
    return s;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseCaseBody() {
    std::vector<std::unique_ptr<Stmt>> out;
    for (;;) {
        if (isP("}") || curT() == TT::End) break;
        if (isKw("case") || isKw("default")) break;
        if (isP(";")) {
            adv();
            continue;
        }
        auto st = parseStmt();
        if (!st) break;
        out.push_back(std::move(st));
    }
    return out;
}
bool Parser::looksLikeLambda(size_t off) const {
    if (curT(off) == TT::Id && curS(off) != "var" && isP("->", off + 1)) return true;
    if (isP("(", off)) {
        int depth = 0;
        size_t k = p + off;
        while (k < ts.size()) {
            const Token& tk = ts[k];
            if (tk.t == TT::Punct) {
                if (tk.s == "(" || tk.s == "[" || tk.s == "{") depth++;
                else if (tk.s == ")" || tk.s == "]" || tk.s == "}") {
                    depth--;
                    if (depth == 0) {
                        size_t n = k + 1;
                        return n < ts.size() && ts[n].t == TT::Punct && ts[n].s == "->";
                    }
                }
            } else if (tk.t == TT::End) return false;
            k++;
        }
        return false;
    }
    return false;
}

std::unique_ptr<Expr> Parser::parseLambdaOrExpr() {
    if (looksLikeLambda(0)) {
        auto e = std::make_unique<Expr>();
        e->k = EK::Lambda;
        e->line = curLine();
        if (isP("(")) {
            adv();
            while (!isP(")") && curT() != TT::End) {
                while (isKw("final")) adv();
                bool typed = false;
                TypeRef t;
                if (isKw("var") && isIdAny(1)) {
                    t.isVar = true;
                    t.name = "var";
                    adv();
                    typed = true;
                } else if ((isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS()))) &&
                           (isIdAny(1) || isP("...", 1))) {
                    t = parseTypeRef(false, false);
                    typed = true;
                }
                if (typed && isP("...")) adv();
                std::string nm = expectId();
                e->lparams.push_back({t, nm});
                if (!eatP(",")) break;
            }
            expectP(")");
        } else {
            std::string nm = expectId();
            e->lparams.push_back({TypeRef(), nm});
        }
        expectP("->");
        if (isP("{")) {
            e->lbodyIsBlock = true;
            adv();
            e->lblock = parseStmtListUntil("}", true);
        } else {
            e->lbody = parseExpr();
        }
        return e;
    }
    return parseAssign();
}

std::unique_ptr<Expr> Parser::parseExpr() {
    if (getenv("JS_DEBUG")) fprintf(stderr, "[parseExpr] at '%s' lambda=%d\n", curS().c_str(), (int)looksLikeLambda(0));
    if (isKw("switch") && isP("(", 1)) {
        auto e = std::make_unique<Expr>();
        e->k = EK::SwitchE;
        e->line = curLine();
        adv();
        expectP("(");
        e->a = parseExpr();
        expectP(")");
        expectP("{");
        while (!isP("}") && curT() != TT::End) {
            if (isP(";")) {
                adv();
                continue;
            }
            CaseArm arm;
            arm.line = curLine();
            arm.arrow = true;
            if (isKw("default")) {
                adv();
                arm.isDefault = true;
            } else {
                eatKw("case");
                for (;;) {
                    if (curT() == TT::Null) {
                        adv();
                        arm.nullLabel = true;
                    } else if (isP("(")) {
                        adv();
                        arm.labels.push_back(parseExpr());
                        expectP(")");
                    } else {
                        size_t save = p;
                        bool patOk = false;
                        TypeRef pt;
                        std::string pn;
                        if ((isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS()))) &&
                            (isIdAny(1) && curS(1) != "when")) {
                            pt = parseTypeRef(false, false);
                            if (isIdAny() && curS() != "when") {
                                pn = expectId();
                                patOk = true;
                            }
                        }
                        if (patOk) {
                            arm.patLabels.push_back(pt);
                            arm.patName = pn;
                        } else {
                            p = save;
                            arm.labels.push_back(parseExprNoLambda());
                        }
                    }
                    if (eatP(",")) continue;
                    break;
                }
            }
            if ((isKw("when") || isId("when"))) {
                adv();
                arm.guard = parseExpr();
            }
            expectP("->");
            if (isP("{")) {
                arm.isBlockVal = true;
                adv();
                arm.body = parseStmtListUntil("}", true);
            } else if (isKw("throw")) {
                adv();
                auto th = std::make_unique<Stmt>();
                th->k = SK::Throw;
                th->line = curLine();
                th->e = parseExpr();
                expectP(";");
                arm.body.push_back(std::move(th));
            } else {
                arm.value = parseExpr();
                expectP(";");
            }
            e->cases.push_back(std::move(arm));
        }
        expectP("}");
        return e;
    }
    if (looksLikeLambda(0)) return parseLambdaOrExpr();
    return parseAssign();
}

std::unique_ptr<Expr> Parser::parseExprNoLambda() { return parseAssign(); }

std::unique_ptr<Expr> Parser::parseAssign() {
    auto lhs = parseTernary();
    static const char* compound[] = {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
                                     "<<=", ">>=", ">>>=", nullptr};
    for (auto* cop : compound) {
        if (cop && isP(cop)) {
            std::string op = cop;
            adv();
            auto e = std::make_unique<Expr>();
            e->k = EK::Assign;
            e->op = op;
            e->line = lhs->line;
            e->a = std::move(lhs);
            e->b = parseExpr();
            return e;
        }
    }
    if (isP("=")) {
        adv();
        auto e = std::make_unique<Expr>();
        e->k = EK::Assign;
        e->op = "=";
        e->line = lhs->line;
        e->a = std::move(lhs);
        e->b = parseExpr();
        return e;
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseTernary() {
    auto cond = parseBinary(0);
    if (isP("?")) {
        adv();
        auto e = std::make_unique<Expr>();
        e->k = EK::Ternary;
        e->line = cond->line;
        e->a = std::move(cond);
        e->b = parseExpr();
        expectP(":");
        e->c = parseExpr();
        return e;
    }
    return cond;
}

int binPrec(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "|") return 3;
    if (op == "^") return 4;
    if (op == "&") return 5;
    if (op == "==" || op == "!=") return 6;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
    if (op == "<<" || op == ">>" || op == ">>>") return 8;
    if (op == "+" || op == "-") return 9;
    if (op == "*" || op == "/" || op == "%") return 10;
    return 0;
}

std::unique_ptr<Expr> Parser::parseBinary(int minPrec) {
    auto lhs = parseUnary();
    for (;;) {
        std::string op;
        int prec = 0;
        if (isKw("instanceof")) {
            op = "instanceof";
            prec = 7;
        } else {
            for (const char* cand : {"||", "&&", "|", "^", "&", "==", "!=", "<", ">",
                                     "<=", ">=", "<<", ">>", ">>>", "+", "-", "*", "/", "%"}) {
                if (isP(cand)) {
                    op = cand;
                    prec = binPrec(cand);
                    break;
                }
            }
        }
        if (prec == 0 || prec < minPrec) break;
        int line = curLine();
        adv();
        if (op == "instanceof") {
            auto e = std::make_unique<Expr>();
            e->k = EK::InstOf;
            e->line = line;
            e->a = std::move(lhs);
            e->ty = parseTypeRef(true, false);
            if (isIdAny()) {
                e->patName = expectId();
            } else if (isP("(")) {
                adv();
                for (;;) {
                    TypeRef ct = parseTypeRef(false, false);
                    std::string cn = expectId();
                    e->patComps.push_back({ct, cn});
                    if (!eatP(",")) break;
                }
                expectP(")");
            }
            lhs = std::move(e);
            continue;
        }
        auto rhs = parseBinary(prec + 1);
        auto e = std::make_unique<Expr>();
        e->k = EK::Bin;
        e->op = op;
        e->line = line;
        e->a = std::move(lhs);
        e->b = std::move(rhs);
        lhs = std::move(e);
    }
    return lhs;
}

int Parser::probeParenType(size_t off, bool* primOut) const {
    size_t k = off + 1;
    int depth = 0;
    bool sawType = false;
    *primOut = false;
    while (k < ts.size()) {
        const Token& tk = ts[k];
        if (tk.t == TT::Punct) {
            if (tk.s == "<") {
                depth++;
                k++;
                continue;
            }
            if (tk.s == ">") {
                if (depth == 0) return -1;
                depth--;
                k++;
                continue;
            }
            if (depth > 0) {
                if (tk.s == "," || tk.s == "?" || tk.s == ">>" || tk.s == ">>>" || tk.s == "." ||
                    (tk.t == TT::Kw && primitiveTypes().count(tk.s))) {
                    if (tk.s == ">>" || tk.s == ">>>") depth -= (int)tk.s.size();
                    k++;
                    continue;
                }
                return -1;
            }
            if (tk.s == ")") return sawType ? (int)k : -1;
            if (tk.s == "[") {
                if (!sawType || k + 1 >= ts.size() || ts[k + 1].t != TT::Punct || ts[k + 1].s != "]")
                    return -1;
                k += 2;
                continue;
            }
            return -1;
        }
        if (tk.t == TT::Kw && primitiveTypes().count(tk.s)) {
            if (depth > 0 || sawType) return -1;
            sawType = true;
            *primOut = true;
            k++;
            continue;
        }
        if (tk.t == TT::Id) {
            if (sawType || depth > 0) return -1;
            sawType = true;
            k++;
            while (k < ts.size() && ts[k].t == TT::Punct && ts[k].s == "." && k + 1 < ts.size() &&
                   ts[k + 1].t == TT::Id) {
                k += 2;
            }
            continue;
        }
        if (tk.t == TT::Kw && (tk.s == "var")) return -1;
        return -1;
    }
    return -1;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    int line = curLine();
    if (isP("(")) {
        bool prim = false;
        int closeIdx = probeParenType(p, &prim);
        if (closeIdx > 0) {
            size_t after = (size_t)closeIdx + 1;
            bool castable = false;
            if (after < ts.size()) {
                const Token& nx = ts[after];
                if (nx.t == TT::Id || nx.t == TT::Int || nx.t == TT::Flt || nx.t == TT::Chr ||
                    nx.t == TT::Str || nx.t == TT::TBlk || nx.t == TT::Bool || nx.t == TT::Null)
                    castable = true;
                else if (nx.t == TT::Punct && (nx.s == "(" || nx.s == "!" || nx.s == "~"))
                    castable = true;
                else if (nx.t == TT::Punct && (nx.s == "++" || nx.s == "--"))
                    castable = true;
                else if (nx.t == TT::Kw)
                    castable = (nx.s != "instanceof");
                else if (nx.t == TT::Punct && (nx.s == "+" || nx.s == "-") && prim)
                    castable = true;
            }
            if (castable) {
                adv();
                auto e = std::make_unique<Expr>();
                e->k = EK::Cast;
                e->line = line;
                e->ty = parseTypeRef(true, false);
                expectP(")");
                e->a = parseUnary();
                return e;
            }
        }
    }
    static const char* pre[] = {"+", "-", "!", "~", "++", "--", nullptr};
    for (auto* op : pre) {
        if (op && isP(op)) {
            adv();
            auto e = std::make_unique<Expr>();
            e->k = EK::Un;
            e->op = op;
            e->line = line;
            e->a = parseUnary();
            return e;
        }
    }
    return parsePostfixExpr();
}

std::unique_ptr<Expr> Parser::parsePostfixExpr() {
    auto e = parsePrimary();
    if (e->k == EK::Ident && isP("(") && e->name != "super") {
        auto call = std::make_unique<Expr>();
        call->k = EK::Call;
        call->line = e->line;
        call->name = e->name;
        adv();
        while (!isP(")") && curT() != TT::End) {
            call->args.push_back(parseExpr());
            if (!eatP(",")) break;
        }
        expectP(")");
        e = std::move(call);
    }
    for (;;) {
        int line = curLine();
        if (isP(".")) {
            if (isKw("class", 1)) {
                adv();
                adv();
                auto cl = std::make_unique<Expr>();
                cl->k = EK::ClassLit;
                cl->line = line;
                cl->ty = e->k == EK::Ident ? e->ty : TypeRef();
                if (e->k == EK::Ident) cl->ty.name = e->name;
                else if (e->k == EK::Field) cl->ty.name = e->name;
                e = std::move(cl);
                continue;
            }
            if (isId("this", 1)) {
                adv();
                adv();
                auto fld = std::make_unique<Expr>();
                fld->k = EK::Field;
                fld->name = "this";
                fld->line = line;
                fld->obj = std::move(e);
                e = std::move(fld);
                continue;
            }
            if (isKw("new", 1) && isP("(", 2)) {
                adv();
                adv();
                e = parseNew();
                continue;
            }
            adv();
            if (isP("<")) {
                size_t save = p;
                std::vector<TypeRef> ta;
                bool ok = parseGenericTypeArgs(ta);
                if (ok && isIdAny() && isP("(", 1)) {
                    std::string nm = expectId();
                    auto call = std::make_unique<Expr>();
                    call->k = EK::Call;
                    call->line = line;
                    call->name = nm;
                    call->obj = std::move(e);
                    call->targs = ta;
                    adv();
                    while (!isP(")") && curT() != TT::End) {
                        call->args.push_back(parseExpr());
                        if (!eatP(",")) break;
                    }
                    expectP(")");
                    e = std::move(call);
                    continue;
                }
                p = save;
            }
            std::string nm = expectId();
            if (isP("(")) {
                auto call = std::make_unique<Expr>();
                call->k = EK::Call;
                call->line = line;
                call->name = nm;
                call->obj = std::move(e);
                adv();
                while (!isP(")") && curT() != TT::End) {
                    call->args.push_back(parseExpr());
                    if (!eatP(",")) break;
                }
                expectP(")");
                e = std::move(call);
                continue;
            }
            auto fld = std::make_unique<Expr>();
            fld->k = EK::Field;
            fld->name = nm;
            fld->line = line;
            fld->obj = std::move(e);
            e = std::move(fld);
            continue;
        }
        if (isP("[")) {
            adv();
            auto idx = std::make_unique<Expr>();
            idx->k = EK::ArrIdx;
            idx->line = line;
            idx->a = std::move(e);
            idx->b = parseExpr();
            expectP("]");
            e = std::move(idx);
            continue;
        }
        if (isP("++") || isP("--")) {
            auto po = std::make_unique<Expr>();
            po->k = EK::Post;
            po->op = curS();
            po->line = line;
            po->postfix = true;
            po->a = std::move(e);
            adv();
            e = std::move(po);
            continue;
        }
        if (isP("::")) {
            adv();
            std::string nm;
            if (isKw("new")) {
                adv();
                nm = "new";
            } else nm = expectId();
            auto mr = std::make_unique<Expr>();
            mr->k = EK::MRef;
            mr->line = line;
            mr->name = nm;
            mr->obj = std::move(e);
            e = std::move(mr);
            continue;
        }
        break;
    }
    return e;
}

bool Parser::parseGenericTypeArgs(std::vector<TypeRef>& out) {
    if (!isP("<")) return false;
    size_t save = p;
    adv();
    int depth = 1;
    bool ok = true;
    while (depth > 0) {
        if (curT() == TT::End) {
            ok = false;
            break;
        }
        if (isP("<")) {
            depth++;
        } else if (isP(">")) {
            depth--;
        } else if (isP(">>") || isP(">>>")) {
            depth -= (int)curS().size();
        } else if (isIdAny() || (curT() == TT::Kw && primitiveTypes().count(curS())) ||
                   isP(",") || isP("?") || isP(".") || isP("extends") || isP("super")) {
        } else {
            ok = false;
            break;
        }
        adv();
    }
    bool hasCallTarget = isP("(") || (isIdAny() && isP("(", 1));
    if (!ok || !hasCallTarget) {
        p = save;
        return false;
    }
    p = save;
    adv();
    depth = 1;
    while (depth > 0) {
        if (isP("<")) depth++;
        else if (isP(">")) depth--;
        else if (isP(">>") || isP(">>>")) depth -= (int)curS().size();
        else if (isP(",")) {
            adv();
            continue;
        } else if (isP("?")) {
            adv();
            if (eatKw("extends") || eatKw("super")) (void)parseTypeRef(false, false);
            continue;
        } else {
            out.push_back(parseTypeRef(false, false));
            continue;
        }
        adv();
    }
    return true;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    int line = curLine();
    TT t = curT();
    const std::string s = curS();
    if (t == TT::Int || t == TT::Flt || t == TT::Chr || t == TT::Str || t == TT::TBlk ||
        t == TT::Bool || t == TT::Null) {
        auto e = std::make_unique<Expr>();
        e->k = EK::Lit;
        e->lt = t;
        e->val = s;
        e->line = line;
        adv();
        return e;
    }
    if (isKw("this")) {
        adv();
        if (isP("(")) {
            auto call = std::make_unique<Expr>();
            call->k = EK::CtorCall;
            call->name = "this";
            call->line = line;
            adv();
            while (!isP(")") && curT() != TT::End) {
                call->args.push_back(parseExpr());
                if (!eatP(",")) break;
            }
            expectP(")");
            return call;
        }
        auto e = std::make_unique<Expr>();
        e->k = EK::This;
        e->line = line;
        return e;
    }
    if (isKw("super")) {
        adv();
        if (isP("(")) {
            auto call = std::make_unique<Expr>();
            call->k = EK::CtorCall;
            call->name = "super";
            call->line = line;
            adv();
            while (!isP(")") && curT() != TT::End) {
                call->args.push_back(parseExpr());
                if (!eatP(",")) break;
            }
            expectP(")");
            return call;
        }
        auto e = std::make_unique<Expr>();
        e->k = EK::Ident;
        e->name = "super";
        e->line = line;
        return e;
    }
    if (isKw("new")) return parseNew();
    if (isP("(")) {
        adv();
        auto e = parseExpr();
        expectP(")");
        return e;
    }
    if (t == TT::Id) {
        auto e = std::make_unique<Expr>();
        e->k = EK::Ident;
        e->name = expectId();
        e->line = line;
        return e;
    }
    ctx.error(line, "unexpected token '" + s + "' in expression");
    adv();
    auto e = std::make_unique<Expr>();
    e->k = EK::Ident;
    e->name = "_";
    e->line = line;
    return e;
}

std::unique_ptr<Expr> Parser::parseNew() {
    int line = curLine();
    adv();
    auto e = std::make_unique<Expr>();
    e->line = line;
    if (isP("[") && isP("]", 1)) {
        e->k = EK::NewArr;
        e->implArr = true;
        adv();
        adv();
        while (isP("[") && isP("]", 1)) {
            adv();
            adv();
            e->ty.dims++;
        }
        if (isP("{")) {
            adv();
            while (!isP("}") && curT() != TT::End) {
                e->args.push_back(parseVarInit());
                if (!eatP(",")) break;
            }
            expectP("}");
        }
        return e;
    }
    e->ty = parseTypeRef(false, false);
    if (isP("[")) {
        e->k = EK::NewArr;
        while (isP("[")) {
            adv();
            if (isP("]")) {
                adv();
                e->ty.dims++;
            } else {
                e->dims.push_back(parseExpr());
                expectP("]");
            }
        }
        if (isP("{")) {
            adv();
            while (!isP("}") && curT() != TT::End) {
                e->args.push_back(parseVarInit());
                if (!eatP(",")) break;
            }
            expectP("}");
        }
        return e;
    }
    if (e->ty.dims > 0 && isP("{")) {
        e->k = EK::NewArr;
        adv();
        while (!isP("}") && curT() != TT::End) {
            e->args.push_back(parseVarInit());
            if (!eatP(",")) break;
        }
        expectP("}");
        return e;
    }
    e->k = EK::New;
    expectP("(");
    while (!isP(")") && curT() != TT::End) {
        e->args.push_back(parseExpr());
        if (!eatP(",")) break;
    }
    expectP(")");
    if (isP("{")) {
        auto body = std::make_unique<AnonBody>();
        body->base = e->ty;
        body->line = line;
        Decl tmp;
        tmp.dk = 0;
        std::vector<std::pair<TypeRef, std::string>> none;
        parseTypeBody(tmp, false, false, &none);
        body->members = std::move(tmp.members);
        e->anon = std::move(body);
    }
    return e;
}
inline std::string lastSeg(const std::string& dotted) {
    size_t dot = dotted.rfind('.');
    return dot == std::string::npos ? dotted : dotted.substr(dot + 1);
}

std::string firstSeg(const std::string& dotted) {
    size_t dot = dotted.find('.');
    return dot == std::string::npos ? dotted : dotted.substr(0, dot);
}

std::string ownerSimple(const std::string& owner) {
    size_t dot = owner.rfind('.');
    return dot == std::string::npos ? owner : owner.substr(dot + 1);
}

TypeInfo toInfo(Ctx& ctx, const TypeRef& t, bool genArg = false) {
    (void)ctx;
    TypeInfo ti;
    if (t.wildcard) {
        if (!t.name.empty()) {
            TypeRef b;
            b.name = t.name;
            b.args = t.args;
            ti = toInfo(ctx, b, genArg);
        }
        return ti;
    }
    if (t.name.empty() || t.isVar) return ti;
    if (t.name == "Optional" && t.args.size() == 1) {
        TypeInfo inner = toInfo(ctx, t.args[0], true);
        if (!inner.name.empty() && !endsWith(inner.name, "?")) inner.name += "?";
        inner.known = true;
        return inner;
    }
    std::string mapped = mapJavaTypeFqn(t.name, genArg);
    if (mapped == "__Scanner") ctx.needScanner = true;
    ti.name = mapped.empty() ? t.name : mapped;
    for (auto& a : t.args) ti.args.push_back(toInfo(ctx, a, true));
    ti.dims = t.dims;
    ti.known = true;
    return ti;
}

const std::set<std::string>& knownInterfaces() {
    static const std::set<std::string> s = {
        "Runnable", "Comparable", "Comparator", "Iterable", "Callable", "Cloneable",
        "Serializable", "AutoCloseable", "Closeable", "Flushable", "Readable",
        "Appendable", "ActionListener", "MouseListener", "KeyListener", "ItemListener",
        "WindowListener", "Externalizable", "EventListener"};
    return s;
}

bool isIfaceType(Ctx& ctx, const TypeRef& t) {
    if (t.name.empty()) return false;
    std::string simple = firstSeg(t.name);
    auto it = ctx.typeByName.find(simple);
    if (it != ctx.typeByName.end()) return it->second->dk == 1;
    std::string mapped = mapJavaTypeFqn(t.name, false);
    if (mapped == "Action" || mapped == "Func" || mapped == "Comparison" ||
        mapped == "Predicate" || mapped == "IEnumerable" || mapped == "IEnumerator" ||
        mapped == "IComparable" || mapped == "ICloneable" || mapped == "IDisposable")
        return true;
    if (knownInterfaces().count(simple)) return true;
    return false;
}

void registerDecl(Ctx& ctx, Decl& d) {
    if (!d.name.empty()) ctx.typeByName[d.name] = &d;
    for (auto& m : d.members)
        if (m.mk == 4 && m.nested) registerDecl(ctx, *m.nested);
}

void collectClassInfo(Ctx& ctx, Decl& d) {
    std::set<std::string>& fnames = ctx.classFieldNames[d.name];
    auto& rets = ctx.methodReturns[d.name];
    for (auto& m : d.members) {
        if (m.mk == 0) {
            for (size_t i = 0; i < m.names.size(); i++) {
                fnames.insert(m.names[i]);
                TypeRef t = m.ty;
                t.dims += m.fdims[i];
                ctx.classFieldType[d.name + "." + m.names[i]] = t;
                if (hasMod(m.mods, "static")) ctx.staticFields.insert(d.name + "." + m.names[i]);
                else ctx.instanceFields.insert(d.name + "." + m.names[i]);
            }
        } else if (m.mk == 1) {
            rets[m.name + "/" + std::to_string(m.params.size())] = m.ty;
            std::vector<TypeRef> ptypes;
            for (auto& p : m.params) ptypes.push_back(p.first);
            ctx.methodParamTypes[d.name + "." + m.name + "/" + std::to_string(m.params.size())] = ptypes;
            if (hasMod(m.mods, "static")) ctx.staticMethods.insert(d.name + "." + m.name + "/" + std::to_string(m.params.size()));
            else ctx.instanceMethods.insert(d.name + "." + m.name + "/" + std::to_string(m.params.size()));
        } else if (m.mk == 2) {
            std::vector<TypeRef> ptypes;
            for (auto& p : m.params) ptypes.push_back(p.first);
            ctx.methodParamTypes[d.name + ".<init>/" + std::to_string(m.params.size())] = ptypes;
        }
        if (m.mk == 4 && m.nested) collectClassInfo(ctx, *m.nested);
    }
}

std::string staticMathName(const std::string& m) {
    static const std::map<std::string, std::string> names = {
        {"abs", "Abs"}, {"max", "Max"}, {"min", "Min"}, {"pow", "Pow"}, {"sqrt", "Sqrt"},
        {"floor", "Floor"}, {"ceil", "Ceiling"}, {"round", "Round"}, {"log", "Log"},
        {"log10", "Log10"}, {"exp", "Exp"}, {"sin", "Sin"}, {"cos", "Cos"}, {"tan", "Tan"},
        {"asin", "Asin"}, {"acos", "Acos"}, {"atan", "Atan"}, {"atan2", "Atan2"},
        {"sinh", "Sinh"}, {"cosh", "Cosh"}, {"tanh", "Tanh"}, {"signum", "Sign"},
        {"hypot", "Hypot"}, {"cbrt", "Cbrt"}, {"toRadians", "ToRadians"},
        {"toDegrees", "ToDegrees"}, {"PI", "PI"}, {"E", "E"}};
    auto it = names.find(m);
    if (it != names.end()) return it->second;
    return m;
}

std::string arraysStaticName(const std::string& m) {
    static const std::map<std::string, std::string> names = {
        {"sort", "Sort"}, {"fill", "Fill"}, {"copyOf", "CopyOf"},
        {"copyOfRange", "CopyOfRange"}, {"toString", "ToString"},
        {"deepToString", "ToString"}, {"equals", "Equals"}, {"hashCode", "HashCode"},
        {"asList", "AsList"}, {"binarySearch", "BinarySearch"}};
    auto it = names.find(m);
    if (it != names.end()) return it->second;
    return m;
}

std::string wrapperConstCs(const std::string& ownerSimple, const std::string& member) {
    static const std::map<std::string, std::string> m = {
        {"MAX_VALUE", "MaxValue"}, {"MIN_VALUE", "MinValue"},
        {"POSITIVE_INFINITY", "PositiveInfinity"}, {"NEGATIVE_INFINITY", "NegativeInfinity"},
        {"NaN", "NaN"}};
    auto& wm = wrapperMap();
    auto wit = wm.find(ownerSimple);
    if (wit == wm.end()) return "";
    auto it = m.find(member);
    if (it == m.end()) return "";
    return mapPrim(wit->second) + "." + it->second;
}

void processImports(Ctx& ctx, Unit& u) {
    for (auto& imp : u.imports) {
        const std::string& path = imp.first;
        bool stat = imp.second;
        if (stat) {
            if (endsWith(path, ".*")) {
                std::string owner = path.substr(0, path.size() - 2);
                std::string simple = ownerSimple(owner);
                if (simple == "Math" && startsWith(owner, "java.lang")) ctx.needStaticMath = true;
                continue;
            }
            size_t dot = path.rfind('.');
            if (dot == std::string::npos) continue;
            std::string owner = path.substr(0, dot);
            std::string member = path.substr(dot + 1);
            std::string osimple = ownerSimple(owner);
            if ((osimple == "Math" || osimple == "StrictMath") && startsWith(owner, "java.lang")) {
                ctx.staticImports[member] = "Math." + staticMathName(member);
            } else if (osimple == "System" && startsWith(owner, "java.lang")) {
                if (member == "out") ctx.staticImports["out"] = "Console.Out";
                else if (member == "err") ctx.staticImports["err"] = "Console.Error";
                else if (member == "in") ctx.staticImports["in"] = "Console.In";
            } else if (osimple == "Objects" && startsWith(owner, "java.util")) {
                ctx.staticImports[member] = "Objects." + member;
            } else if (osimple == "Arrays" && startsWith(owner, "java.util")) {
                static const std::map<std::string, std::string> am = {
                    {"sort", "Array.Sort"}, {"fill", "Array.Fill"},
                    {"binarySearch", "Array.BinarySearch"},
                    {"asList", "__JHelp.AsList"}, {"toString", "__JHelp.ArrayToString"},
                    {"deepToString", "__JHelp.ArrayToString"}, {"equals", "__JHelp.ArrayEquals"},
                    {"copyOf", "__JHelp.CopyOf"}, {"copyOfRange", "__JHelp.CopyOfRange"},
                    {"hashCode", "Array.Empty<int>().GetHashCode"}};
                auto it = am.find(member);
                if (it != am.end()) ctx.staticImports[member] = it->second;
            } else if (osimple == "Collections" && startsWith(owner, "java.util")) {
                static const std::map<std::string, std::string> cm = {
                    {"sort", "__JHelp.Sort"}, {"reverse", "__JHelp.Reverse"},
                    {"max", "Enumerable.Max"}, {"min", "Enumerable.Min"},
                    {"swap", "__JHelp.Swap"}};
                auto it = cm.find(member);
                if (it != cm.end()) ctx.staticImports[member] = it->second;
            } else {
                std::string wc = wrapperConstCs(osimple, member);
                if (!wc.empty()) ctx.staticImports[member] = wc;
                else if (startsWith(owner, "java.")) {
                    continue;
                } else {
                    ctx.staticImports[member] = osimple + "." + member;
                }
            }
            continue;
        }
        std::string pkg = path;
        if (endsWith(pkg, ".*")) pkg = pkg.substr(0, pkg.size() - 2);
        else {
            size_t lastDot = pkg.rfind('.');
            if (lastDot != std::string::npos) pkg = pkg.substr(0, lastDot);
            else pkg = "";
        }
        if (pkg.empty()) continue;
        std::string csns = csNamespaceForJavaPkg(pkg);
        if (csns != pkg) {
            ctx.usings.insert(csns);
            continue;
        }
        if (startsWith(pkg, "java.") || startsWith(pkg, "javax.")) {
            size_t d1 = pkg.find('.', 5);
            std::string top2 = d1 == std::string::npos ? pkg : pkg.substr(0, d1);
            std::string mapped = csNamespaceForJavaPkg(top2);
            if (mapped != top2) ctx.usings.insert(mapped);
            continue;
        }
        ctx.usings.insert(pkg);
    }
}

std::string ancestorOf(Ctx& ctx, const std::string& cls) {
    auto it = ctx.typeByName.find(cls);
    if (it == ctx.typeByName.end()) return "";
    Decl* d = it->second;
    if (d->extends.name.empty()) return "";
    std::string base = firstSeg(d->extends.name);
    if (ctx.typeByName.count(base)) return base;
    return "";
}

bool isSubclassOf(Ctx& ctx, const std::string& cls, const std::string& base) {
    std::string c = cls;
    int guard = 0;
    while (!c.empty() && guard++ < 32) {
        if (c == base) return true;
        c = ancestorOf(ctx, c);
    }
    return false;
}

void resolveOverrides(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) {
        if (d->dk == 2) continue;
        for (auto& m : d->members) {
            if (m.mk != 1 || hasMod(m.mods, "static") || hasMod(m.mods, "private")) continue;
            std::string anc = ancestorOf(ctx, d->name);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                Decl* ad = ctx.typeByName[anc];
                bool found = false;
                for (auto& am : ad->members) {
                    if (am.mk != 1) continue;
                    if (am.name == m.name && am.params.size() == m.params.size() &&
                        !hasMod(am.mods, "static") && !hasMod(am.mods, "private")) {
                        ctx.virtualM[anc].insert(m.name + "/" + std::to_string(m.params.size()));
                        ctx.overrideM[d->name].insert(m.name + "/" + std::to_string(m.params.size()));
                        found = true;
                        break;
                    }
                }
                if (found) break;
                anc = ancestorOf(ctx, anc);
            }
        }
        for (auto& m : d->members)
            if (m.mk == 4 && m.nested) {
                Unit tmp;
                tmp.types.push_back(std::unique_ptr<Decl>(m.nested.get()));
                resolveOverrides(ctx, tmp);
                tmp.types[0].release();
            }
    }
}

void detectSamDelegatesDecl(Ctx& ctx, Decl& d, std::set<std::string>& candidates);
void collectDeclsRecursive(Decl& d, std::vector<Decl*>& out);
void scanAnonBasesExpr(const Expr& e, std::set<std::string>& out);
void scanAnonBasesStmt(const Stmt& st, std::set<std::string>& out);
void scanAnonBasesDeclBody(Decl& d, std::set<std::string>& out);
bool refsCaptureOrThis(const Expr& e, const std::set<std::string>& names);
bool refsCaptureOrThisStmt(const Stmt& st, const std::set<std::string>& names);
void appendFieldInitsToCtor(Member& ctor, const std::vector<std::pair<std::string, std::unique_ptr<Expr>>>& inits,
                            const std::set<std::string>& capNames);
std::unique_ptr<Expr> cloneExpr(const Expr& e);

void scanAnonBasesStmt(const Stmt& st, std::set<std::string>& out) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& x : st.inits)
                if (x) scanAnonBasesExpr(*x, out);
            break;
        case SK::Block:
            for (auto& s : st.block) scanAnonBasesStmt(*s, out);
            break;
        case SK::ExprStmt:
        case SK::Return:
        case SK::Throw:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            break;
        case SK::If:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            if (st.body2) scanAnonBasesStmt(*st.body2, out);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::For:
            for (auto& x : st.inits)
                if (x) scanAnonBasesExpr(*x, out);
            for (auto& x : st.initExprs)
                if (x) scanAnonBasesExpr(*x, out);
            for (auto& x : st.updateExprs)
                if (x) scanAnonBasesExpr(*x, out);
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::Foreach:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::Try:
            for (auto& x : st.resInits)
                if (x) scanAnonBasesExpr(*x, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            for (auto& c : st.catches) scanAnonBasesStmt(*c.body, out);
            if (st.fin) scanAnonBasesStmt(*st.fin, out);
            break;
        case SK::Switch:
            if (st.subject) scanAnonBasesExpr(*st.subject, out);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) scanAnonBasesExpr(*l, out);
                if (arm.guard) scanAnonBasesExpr(*arm.guard, out);
                if (arm.value) scanAnonBasesExpr(*arm.value, out);
                for (auto& s : arm.body) scanAnonBasesStmt(*s, out);
            }
            break;
        case SK::Sync:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::Assert:
            if (st.e) scanAnonBasesExpr(*st.e, out);
            if (st.e2) scanAnonBasesExpr(*st.e2, out);
            break;
        case SK::Label:
            if (st.body) scanAnonBasesStmt(*st.body, out);
            break;
        case SK::LocalClass:
            if (st.lclass) scanAnonBasesDeclBody(*st.lclass, out);
            break;
        default:
            break;
    }
}

void scanAnonBasesDeclBody(Decl& d, std::set<std::string>& out);

void scanAnonBasesExpr(const Expr& e, std::set<std::string>& out) {
    if (e.k == EK::New && e.anon && !e.ty.name.empty()) out.insert(firstSeg(e.ty.name));
    if (e.obj) scanAnonBasesExpr(*e.obj, out);
    if (e.a) scanAnonBasesExpr(*e.a, out);
    if (e.b) scanAnonBasesExpr(*e.b, out);
    if (e.c) scanAnonBasesExpr(*e.c, out);
    if (e.lbody) scanAnonBasesExpr(*e.lbody, out);
    for (auto& x : e.args)
        if (x) scanAnonBasesExpr(*x, out);
    for (auto& x : e.dims)
        if (x) scanAnonBasesExpr(*x, out);
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock) scanAnonBasesStmt(*s, out);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) scanAnonBasesExpr(*l, out);
        if (arm.guard) scanAnonBasesExpr(*arm.guard, out);
        if (arm.value) scanAnonBasesExpr(*arm.value, out);
        for (auto& s : arm.body) scanAnonBasesStmt(*s, out);
    }
    if (e.anon) {
        for (auto& m : e.anon->members) {
            for (auto& f : m.finits)
                if (f) scanAnonBasesExpr(*f, out);
            if (m.body) scanAnonBasesStmt(*m.body, out);
        }
    }
}

void scanAnonBasesDeclBody(Decl& d, std::set<std::string>& out) {
    for (auto& m : d.members) {
        for (auto& f : m.finits)
            if (f) scanAnonBasesExpr(*f, out);
        if (m.body) scanAnonBasesStmt(*m.body, out);
        if (m.nested) scanAnonBasesDeclBody(*m.nested, out);
    }
    for (auto& c : d.consts) {
        for (auto& a : c.args)
            if (a) scanAnonBasesExpr(*a, out);
        if (c.body) scanAnonBasesDeclBody(*c.body, out);
    }
}

void detectSamDelegates(Ctx& ctx, Unit& u) {
    std::set<std::string> candidates;
    for (auto& d : u.types) detectSamDelegatesDecl(ctx, *d, candidates);
    std::set<std::string> anonBases;
    for (auto& d : u.types) scanAnonBasesDeclBody(*d, anonBases);
    std::vector<Decl*> all;
    for (auto& d : u.types) collectDeclsRecursive(*d, all);
    for (Decl* d : all) {
        std::vector<std::string> refs;
        for (auto& t : d->impls) refs.push_back(firstSeg(t.name));
        if (!d->extends.empty()) refs.push_back(firstSeg(d->extends.name));
        for (auto& r : refs)
            if (candidates.count(r) && r != d->name) candidates.erase(r);
    }
    for (const auto& r : anonBases) candidates.erase(r);
    ctx.samDelegates = candidates;
}

void collectDeclsRecursive(Decl& d, std::vector<Decl*>& out) {
    out.push_back(&d);
    for (auto& m : d.members)
        if (m.mk == 4 && m.nested) collectDeclsRecursive(*m.nested, out);
}

void detectSamDelegatesDecl(Ctx& ctx, Decl& d, std::set<std::string>& candidates) {
    {
        if (d.dk == 1 && mapJavaTypeFqn(d.name, true).empty()) {
            int abstractCount = 0;
            bool ok = true;
            for (auto& m : d.members) {
                if (m.mk == 1) {
                    if (!m.body) abstractCount++;
                    else if (!hasMod(m.mods, "static") && !hasMod(m.mods, "private")) ok = false;
                } else if (m.mk != 4) {
                    ok = false;
                }
            }
            if (abstractCount == 1 && ok) {
                candidates.insert(d.name);
                for (auto& m : d.members)
                    if (m.mk == 1 && !m.body) ctx.samMethodNames[d.name] = m.name;
            }
        }
        for (auto& m : d.members)
            if (m.mk == 4 && m.nested) detectSamDelegatesDecl(ctx, *m.nested, candidates);
    }
}

void classifyTypesDecl(Ctx& ctx, Decl& d) {
    if (d.dk == 2) {
        bool simple = d.impls.empty() && d.members.empty();
        for (auto& c : d.consts)
            if (!c.args.empty() || c.body) simple = false;
        if (simple) ctx.simpleEnums.insert(d.name);
        else ctx.enumClassTypes.insert(d.name);
    }
    if (d.dk == 3) {
        ctx.recordTypes.insert(d.name);
        for (auto& m : d.members)
            if (m.mk == 2 && hasMod(m.mods, "__recordctor") && !m.compactCtor)
                for (auto& p : m.params) ctx.recordComps[d.name].push_back(p.second);
    }
    for (auto& c : d.consts) ctx.enumConstOwner[c.name] = d.name;
    for (auto& m : d.members)
        if (m.mk == 4 && m.nested) classifyTypesDecl(ctx, *m.nested);
}

void classifyTypes(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) classifyTypesDecl(ctx, *d);
}

void countMains(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) {
        for (auto& m : d->members) {
            if (m.mk == 1 && m.name == "main" && hasMod(m.mods, "static")) ctx.mainCount++;
        }
    }
}
TypeRef infoToTypeRef(const TypeInfo& ti) {
    TypeRef t;
    t.name = ti.name.empty() ? "object" : ti.name;
    for (auto& a : ti.args) t.args.push_back(infoToTypeRef(a));
    t.dims = ti.dims;
    return t;
}

struct LocalClassInfo {
    std::string orig;
    std::string gen;
    std::string enclosingCls;
    std::vector<VarInfo> caps;
    bool needThis = false;
};

struct HEnv {
    Ctx& ctx;
    Decl* enclosing;
    std::string enclosingCls;
    std::vector<VarInfo> methodVars;
    std::set<std::string> outerMembers;
    std::map<std::string, LocalClassInfo> localClassMap;
    std::map<std::string, std::string> outerFieldPaths;
    std::map<std::string, std::string> outerMethodPaths;
    std::string currentLocalClassName;
    bool hasEnclosingInstance = false;
    bool hasRet = false;
    TypeRef methodRet;
    int counter = 0;

    HEnv(Ctx& c, Decl* enc, const std::string& cls) : ctx(c), enclosing(enc), enclosingCls(cls) {}
};

void hoistInExpr(Expr& e, HEnv& env);
void hoistInStmt(Stmt& st, HEnv& env);

const std::set<std::string>& staticReceivers() {
    static const std::set<std::string> s = {
        "Math", "System", "Integer", "Long", "Short", "Byte", "Double", "Float",
        "Boolean", "Character", "String", "Objects", "Arrays", "Collections", "Thread",
        "Optional", "UUID", "Instant", "LocalDate", "LocalTime", "LocalDateTime",
        "Duration", "Period", "Pattern", "Matcher", "Files", "Paths", "Locale",
        "StandardCharsets", "Charset", "BigDecimal", "BigInteger", "Stream",
        "Collectors", "StrictMath", "Console", "Enum", "Void", "StringBuilder"};
    return s;
}

void collectLocalsStmt(const Stmt& st, Ctx& ctx, std::vector<VarInfo>& out);

void collectLocalsExpr(const Expr& e, Ctx& ctx, std::vector<VarInfo>& out) {
    if (e.k == EK::Lambda) {
        for (auto& lp : e.lparams) out.push_back({lp.second, toInfo(ctx, lp.first)});
    }
    if (e.obj) collectLocalsExpr(*e.obj, ctx, out);
    if (e.a) collectLocalsExpr(*e.a, ctx, out);
    if (e.b) collectLocalsExpr(*e.b, ctx, out);
    if (e.c) collectLocalsExpr(*e.c, ctx, out);
    if (e.lbody) collectLocalsExpr(*e.lbody, ctx, out);
    for (auto& x : e.args)
        if (x) collectLocalsExpr(*x, ctx, out);
    for (auto& x : e.dims)
        if (x) collectLocalsExpr(*x, ctx, out);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) collectLocalsExpr(*l, ctx, out);
        if (arm.guard) collectLocalsExpr(*arm.guard, ctx, out);
        if (arm.value) collectLocalsExpr(*arm.value, ctx, out);
        for (auto& s : arm.body) collectLocalsStmt(*s, ctx, out);
    }
}

void collectLocalsStmt(const Stmt& st, Ctx& ctx, std::vector<VarInfo>& out) {
    switch (st.k) {
        case SK::LocalVar:
            for (size_t i = 0; i < st.names.size(); i++) {
                TypeRef t = st.ty;
                t.dims += st.dims[i];
                out.push_back({st.names[i], toInfo(ctx, t)});
                if (st.inits[i]) collectLocalsExpr(*st.inits[i], ctx, out);
            }
            break;
        case SK::Block:
            for (auto& s : st.block) collectLocalsStmt(*s, ctx, out);
            break;
        case SK::ExprStmt:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            break;
        case SK::If:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            if (st.body2) collectLocalsStmt(*st.body2, ctx, out);
            break;
        case SK::While:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::DoWhile:
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            break;
        case SK::For:
            for (size_t i = 0; i < st.names.size(); i++) {
                TypeRef t = st.ty;
                t.dims += st.dims[i];
                out.push_back({st.names[i], toInfo(ctx, t)});
            }
            for (auto& x : st.inits)
                if (x) collectLocalsExpr(*x, ctx, out);
            for (auto& x : st.initExprs)
                if (x) collectLocalsExpr(*x, ctx, out);
            for (auto& x : st.updateExprs)
                if (x) collectLocalsExpr(*x, ctx, out);
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::Foreach:
            for (auto& n : st.names) out.push_back({n, toInfo(ctx, st.ty)});
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::Return:
        case SK::Throw:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            break;
        case SK::Try:
            for (size_t i = 0; i < st.resources.size(); i++) {
                out.push_back({st.resources[i].second, toInfo(ctx, st.resources[i].first)});
                if (st.resInits[i]) collectLocalsExpr(*st.resInits[i], ctx, out);
            }
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            for (auto& c : st.catches) {
                std::string vt = c.types.empty() ? "Exception" : c.types[0].text();
                TypeRef vt2;
                vt2.name = vt;
                out.push_back({c.var, toInfo(ctx, vt2)});
                collectLocalsStmt(*c.body, ctx, out);
            }
            if (st.fin) collectLocalsStmt(*st.fin, ctx, out);
            break;
        case SK::Switch:
            if (st.subject) collectLocalsExpr(*st.subject, ctx, out);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) collectLocalsExpr(*l, ctx, out);
                if (arm.guard) collectLocalsExpr(*arm.guard, ctx, out);
                if (arm.value) collectLocalsExpr(*arm.value, ctx, out);
                for (auto& s : arm.body) collectLocalsStmt(*s, ctx, out);
            }
            break;
        case SK::Sync:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        case SK::Assert:
            if (st.e) collectLocalsExpr(*st.e, ctx, out);
            if (st.e2) collectLocalsExpr(*st.e2, ctx, out);
            break;
        case SK::Label:
            if (st.body) collectLocalsStmt(*st.body, ctx, out);
            break;
        default:
            break;
    }
}

void walkIdentsExpr(const Expr& e, const std::set<std::string>& typeNames,
                    std::set<std::string>& declared, std::set<std::string>& used,
                    bool* needThisRef);

void walkIdentsStmt(const Stmt& st, const std::set<std::string>& typeNames,
                    std::set<std::string>& declared, std::set<std::string>& used,
                    bool* needThisRef) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& n : st.names) declared.insert(n);
            for (auto& x : st.inits)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            break;
        case SK::Block:
            for (auto& s : st.block) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
            break;
        case SK::ExprStmt:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            break;
        case SK::If:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            if (st.body2) walkIdentsStmt(*st.body2, typeNames, declared, used, needThisRef);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::For:
            for (auto& n : st.names) declared.insert(n);
            for (auto& x : st.inits)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            for (auto& x : st.initExprs)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            for (auto& x : st.updateExprs)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::Foreach:
            for (auto& n : st.names) declared.insert(n);
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::Return:
        case SK::Throw:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            break;
        case SK::Break:
        case SK::Continue:
        case SK::Empty:
            break;
        case SK::Try:
            for (auto& r : st.resources) declared.insert(r.second);
            for (auto& x : st.resInits)
                if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            for (auto& c : st.catches) {
                declared.insert(c.var);
                walkIdentsStmt(*c.body, typeNames, declared, used, needThisRef);
            }
            if (st.fin) walkIdentsStmt(*st.fin, typeNames, declared, used, needThisRef);
            break;
        case SK::Switch:
            if (st.subject) walkIdentsExpr(*st.subject, typeNames, declared, used, needThisRef);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) walkIdentsExpr(*l, typeNames, declared, used, needThisRef);
                if (arm.guard) walkIdentsExpr(*arm.guard, typeNames, declared, used, needThisRef);
                if (arm.value) walkIdentsExpr(*arm.value, typeNames, declared, used, needThisRef);
                if (!arm.patName.empty()) declared.insert(arm.patName);
                for (auto& s : arm.body) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
            }
            break;
        case SK::Sync:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::Assert:
            if (st.e) walkIdentsExpr(*st.e, typeNames, declared, used, needThisRef);
            if (st.e2) walkIdentsExpr(*st.e2, typeNames, declared, used, needThisRef);
            break;
        case SK::Label:
            if (st.body) walkIdentsStmt(*st.body, typeNames, declared, used, needThisRef);
            break;
        case SK::LocalClass:
            if (st.lclass) declared.insert(st.lclass->name);
            break;
    }
}

void walkIdentsExpr(const Expr& e, const std::set<std::string>& typeNames,
                    std::set<std::string>& declared, std::set<std::string>& used,
                    bool* needThisRef) {
    if (e.k == EK::Ident) {
        std::string seg = firstSeg(e.name);
        if (seg != "this" && seg != "super" && !typeNames.count(seg) &&
            !staticReceivers().count(seg) && !primitiveTypes().count(seg) &&
            seg != "var" && seg != "record" && seg != "yield" && seg != "sealed")
            used.insert(seg);
        return;
    }
    if (e.k == EK::Call && !e.obj) {
        if (e.name != "this" && e.name != "super") used.insert(firstSeg(e.name));
    }
    if (e.k == EK::Field && e.name == "this" && e.obj) {
        *needThisRef = true;
        return;
    }
    if (e.k == EK::Lambda) {
        for (auto& lp : e.lparams) declared.insert(lp.second);
    }
    if (e.k == EK::InstOf) {
        if (!e.patName.empty()) declared.insert(e.patName);
        for (auto& pc : e.patComps) declared.insert(pc.second);
    }
    if (e.obj) walkIdentsExpr(*e.obj, typeNames, declared, used, needThisRef);
    if (e.a) walkIdentsExpr(*e.a, typeNames, declared, used, needThisRef);
    if (e.b) walkIdentsExpr(*e.b, typeNames, declared, used, needThisRef);
    if (e.c) walkIdentsExpr(*e.c, typeNames, declared, used, needThisRef);
    if (e.lbody) walkIdentsExpr(*e.lbody, typeNames, declared, used, needThisRef);
    for (auto& x : e.args)
        if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
    for (auto& x : e.dims)
        if (x) walkIdentsExpr(*x, typeNames, declared, used, needThisRef);
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) walkIdentsExpr(*l, typeNames, declared, used, needThisRef);
        if (arm.guard) walkIdentsExpr(*arm.guard, typeNames, declared, used, needThisRef);
        if (arm.value) walkIdentsExpr(*arm.value, typeNames, declared, used, needThisRef);
        if (!arm.patName.empty()) declared.insert(arm.patName);
        for (auto& s : arm.body) walkIdentsStmt(*s, typeNames, declared, used, needThisRef);
    }
    if (e.anon) {
        for (auto& m : e.anon->members) {
            for (auto& n : m.names) declared.insert(n);
            if (m.mk == 1) declared.insert(m.name);
            for (auto& p : m.params) declared.insert(p.second);
            for (auto& tp : m.tparams) declared.insert(tp.name);
            for (auto& f : m.finits)
                if (f) walkIdentsExpr(*f, typeNames, declared, used, needThisRef);
            if (m.body) walkIdentsStmt(*m.body, typeNames, declared, used, needThisRef);
        }
    }
}

std::unique_ptr<Expr> mkIdent(const std::string& name, int line = 0) {
    auto e = std::make_unique<Expr>();
    e->k = EK::Ident;
    e->name = name;
    e->line = line;
    return e;
}

void makeFieldChain(const std::string& dotted, std::unique_ptr<Expr> base, Expr& out) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        size_t dot = dotted.find('.', start);
        if (dot == std::string::npos) {
            parts.push_back(dotted.substr(start));
            break;
        }
        parts.push_back(dotted.substr(start, dot - start));
        start = dot + 1;
    }
    out.k = EK::Field;
    out.name = parts.back();
    out.obj = std::move(base);
    for (size_t i = parts.size() - 1; i > 0; i--) {
        auto mid = std::make_unique<Expr>();
        mid->k = EK::Field;
        mid->name = parts[i];
        mid->obj = std::move(out.obj);
        out.obj = std::move(mid);
    }
}

std::unique_ptr<Expr> captureBaseExpr(const std::string& path, int line) {
    std::unique_ptr<Expr> base = mkIdent("__this", line);
    size_t start = 0;
    while (start < path.size()) {
        size_t dot = path.find('.', start);
        std::string part = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!part.empty()) {
            auto field = std::make_unique<Expr>();
            field->k = EK::Field;
            field->name = part;
            field->obj = std::move(base);
            base = std::move(field);
        }
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return base;
}

void rewriteAnonRefsExpr(Expr& e, const std::set<std::string>& capFields,
                         const std::set<std::string>& capMethods, bool hasThis,
                         const std::map<std::string, std::string>* fieldPaths = nullptr,
                         const std::map<std::string, std::string>* methodPaths = nullptr);

void rewriteAnonRefsStmt(Stmt& st, const std::set<std::string>& capFields,
                         const std::set<std::string>& capMethods, bool hasThis,
                         const std::map<std::string, std::string>* fieldPaths = nullptr,
                         const std::map<std::string, std::string>* methodPaths = nullptr) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& x : st.inits)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Block:
            for (auto& s : st.block) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::ExprStmt:
        case SK::Return:
        case SK::Throw:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::If:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body2) rewriteAnonRefsStmt(*st.body2, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::For:
            for (auto& x : st.inits)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& x : st.initExprs)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& x : st.updateExprs)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Foreach:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Try:
            for (auto& x : st.resInits)
                if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& c : st.catches) rewriteAnonRefsStmt(*c.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.fin) rewriteAnonRefsStmt(*st.fin, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Switch:
            if (st.subject) rewriteAnonRefsExpr(*st.subject, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) rewriteAnonRefsExpr(*l, capFields, capMethods, hasThis, fieldPaths, methodPaths);
                if (arm.guard) rewriteAnonRefsExpr(*arm.guard, capFields, capMethods, hasThis, fieldPaths, methodPaths);
                if (arm.value) rewriteAnonRefsExpr(*arm.value, capFields, capMethods, hasThis, fieldPaths, methodPaths);
                for (auto& s : arm.body) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            }
            break;
        case SK::Sync:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Assert:
            if (st.e) rewriteAnonRefsExpr(*st.e, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            if (st.e2) rewriteAnonRefsExpr(*st.e2, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        case SK::Label:
            if (st.body) rewriteAnonRefsStmt(*st.body, capFields, capMethods, hasThis, fieldPaths, methodPaths);
            break;
        default:
            break;
    }
}

void rewriteAnonRefsExpr(Expr& e, const std::set<std::string>& capFields,
                         const std::set<std::string>& capMethods, bool hasThis,
                         const std::map<std::string, std::string>* fieldPaths,
                         const std::map<std::string, std::string>* methodPaths) {
    if (e.k == EK::Ident && hasThis) {
        std::string seg = firstSeg(e.name);
        if (capFields.count(seg)) {
            if (e.name.size() > seg.size() && e.name[seg.size()] == '.') {
                std::string rest = e.name.substr(seg.size() + 1);
                std::string path;
                if (fieldPaths) {
                    auto it = fieldPaths->find(seg);
                    if (it != fieldPaths->end()) path = it->second;
                }
                auto outer = std::make_unique<Expr>();
                outer->k = EK::Field;
                outer->name = seg;
                outer->obj = captureBaseExpr(path, e.line);
                makeFieldChain(rest, std::move(outer), e);
            } else {
                std::string path;
                if (fieldPaths) {
                    auto it = fieldPaths->find(seg);
                    if (it != fieldPaths->end()) path = it->second;
                }
                e.k = EK::Field;
                e.obj = captureBaseExpr(path, e.line);
            }
            return;
        }
    }
    if (e.k == EK::Call && !e.obj && hasThis && capMethods.count(e.name)) {
        std::string path;
        if (methodPaths) {
            auto it = methodPaths->find(e.name);
            if (it != methodPaths->end()) path = it->second;
        }
        e.obj = captureBaseExpr(path, e.line);
        return;
    }
    if (e.k == EK::Field && e.name == "this" && e.obj && hasThis) {
        e.k = EK::Ident;
        e.name = "__this";
        e.obj.reset();
        return;
    }
    if (e.obj) rewriteAnonRefsExpr(*e.obj, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.a) rewriteAnonRefsExpr(*e.a, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.b) rewriteAnonRefsExpr(*e.b, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.c) rewriteAnonRefsExpr(*e.c, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.lbody) rewriteAnonRefsExpr(*e.lbody, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    for (auto& x : e.args)
        if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    for (auto& x : e.dims)
        if (x) rewriteAnonRefsExpr(*x, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) rewriteAnonRefsExpr(*l, capFields, capMethods, hasThis, fieldPaths, methodPaths);
        if (arm.guard) rewriteAnonRefsExpr(*arm.guard, capFields, capMethods, hasThis, fieldPaths, methodPaths);
        if (arm.value) rewriteAnonRefsExpr(*arm.value, capFields, capMethods, hasThis, fieldPaths, methodPaths);
        for (auto& s : arm.body) rewriteAnonRefsStmt(*s, capFields, capMethods, hasThis, fieldPaths, methodPaths);
    }
}

std::set<std::string> chainInstanceFields(Ctx& ctx, const std::string& cls) {
    std::set<std::string> out;
    std::string c = cls;
    int guard = 0;
    while (!c.empty() && guard++ < 32) {
        std::string prefix = c + ".";
        for (auto& f : ctx.instanceFields)
            if (startsWith(f, prefix)) out.insert(f.substr(prefix.size()));
        c = ancestorOf(ctx, c);
    }
    return out;
}

std::set<std::string> chainInstanceMethods(Ctx& ctx, const std::string& cls) {
    std::set<std::string> out;
    std::string c = cls;
    int guard = 0;
    while (!c.empty() && guard++ < 32) {
        std::string prefix = c + ".";
        for (auto& f : ctx.instanceMethods) {
            if (startsWith(f, prefix)) {
                std::string rest = f.substr(prefix.size());
                size_t slash = rest.find('/');
                out.insert(slash == std::string::npos ? rest : rest.substr(0, slash));
            }
        }
        c = ancestorOf(ctx, c);
    }
    return out;
}

struct CapResult {
    std::vector<VarInfo> capLocals;
    std::set<std::string> capFieldNames;
    std::set<std::string> capMethodNames;
    std::map<std::string, std::string> fieldPaths;
    std::map<std::string, std::string> methodPaths;
    bool needThis = false;
};

CapResult computeCaptures(HEnv& env, const std::set<std::string>& declared,
                          const std::set<std::string>& used, bool needThisRef) {
    CapResult r;
    std::map<std::string, VarInfo> vars;
    for (auto& v : env.methodVars) vars[v.name] = v;
    std::set<std::string> instFields = chainInstanceFields(env.ctx, env.enclosingCls);
    std::set<std::string> instMethods = chainInstanceMethods(env.ctx, env.enclosingCls);
    for (const auto& entry : env.outerFieldPaths) instFields.insert(entry.first);
    for (const auto& entry : env.outerMethodPaths) instMethods.insert(entry.first);
    for (auto& n : used) {
        if (declared.count(n)) continue;
        if (vars.count(n)) {
            r.capLocals.push_back(vars[n]);
            continue;
        }
        if (instFields.count(n)) {
            r.capFieldNames.insert(n);
            auto path = env.outerFieldPaths.find(n);
            r.fieldPaths[n] = path == env.outerFieldPaths.end() ? "" : path->second;
        } else if (instMethods.count(n)) {
            r.capMethodNames.insert(n);
            auto path = env.outerMethodPaths.find(n);
            r.methodPaths[n] = path == env.outerMethodPaths.end() ? "" : path->second;
        }
    }
    r.needThis = needThisRef || !r.capFieldNames.empty() || !r.capMethodNames.empty();
    return r;
}

void addCaptureMembers(Decl& g, const CapResult& caps, HEnv& env) {
    for (auto& v : caps.capLocals) {
        Member fm;
        fm.mk = 0;
        fm.mods = {"private", "readonly"};
        fm.ty = infoToTypeRef(v.ty);
        fm.names.push_back(v.name);
        fm.fdims.push_back(0);
        fm.finits.push_back(nullptr);
        g.members.push_back(std::move(fm));
    }
    if (caps.needThis) {
        Member fm;
        fm.mk = 0;
        fm.mods = {"private", "readonly"};
        fm.ty.name = env.enclosingCls;
        fm.names.push_back("__this");
        fm.fdims.push_back(0);
        fm.finits.push_back(nullptr);
        g.members.push_back(std::move(fm));
    }
}

Member buildCaptureCtor(const std::string& clsName, const CapResult& caps,
                        std::vector<std::unique_ptr<Expr>> baseArgs, const std::string& outerCls) {
    Member cm;
    cm.mk = 2;
    cm.name = clsName;
    cm.mods = {"public"};
    for (auto& v : caps.capLocals) cm.params.push_back({infoToTypeRef(v.ty), v.name});
    if (caps.needThis) {
        TypeRef ot;
        ot.name = outerCls.empty() ? "object" : outerCls;
        cm.params.push_back({ot, "__this"});
    }
    cm.baseArgs = std::move(baseArgs);
    auto blk = std::make_unique<Stmt>();
    blk->k = SK::Block;
    for (auto& v : caps.capLocals) {
        auto as = std::make_unique<Stmt>();
        as->k = SK::ExprStmt;
        auto tgt = std::make_unique<Expr>();
        tgt->k = EK::Field;
        tgt->name = v.name;
        tgt->obj = mkIdent("this");
        auto val = mkIdent(v.name);
        auto call = std::make_unique<Expr>();
        call->k = EK::Assign;
        call->op = "=";
        call->a = std::move(tgt);
        call->b = std::move(val);
        as->e = std::move(call);
        blk->block.push_back(std::move(as));
    }
    if (caps.needThis) {
        auto as = std::make_unique<Stmt>();
        as->k = SK::ExprStmt;
        auto tgt = std::make_unique<Expr>();
        tgt->k = EK::Field;
        tgt->name = "__this";
        tgt->obj = mkIdent("this");
        auto call = std::make_unique<Expr>();
        call->k = EK::Assign;
        call->op = "=";
        call->a = std::move(tgt);
        call->b = mkIdent("__this");
        as->e = std::move(call);
        blk->block.push_back(std::move(as));
    }
    cm.body = std::move(blk);
    return cm;
}

void processAnon(Expr& ne, HEnv& env) {
    AnonBody& body = *ne.anon;
    for (auto& m : body.members) {
        for (auto& f : m.finits)
            if (f) hoistInExpr(*f, env);
        if (m.body) hoistInStmt(*m.body, env);
    }
    std::set<std::string> typeNames;
    for (auto& kv : env.ctx.typeByName) typeNames.insert(kv.first);
    std::set<std::string> declared, used;
    bool needThisRef = false;
    for (auto& m : body.members) {
        for (auto& n : m.names) declared.insert(n);
        if (m.mk == 1) declared.insert(m.name);
        for (auto& p : m.params) declared.insert(p.second);
        for (auto& tp : m.tparams) declared.insert(tp.name);
        for (auto& f : m.finits)
            if (f) walkIdentsExpr(*f, typeNames, declared, used, &needThisRef);
        if (m.body) walkIdentsStmt(*m.body, typeNames, declared, used, &needThisRef);
    }
    CapResult caps = computeCaptures(env, declared, used, needThisRef);
    caps.needThis = caps.needThis || env.hasEnclosingInstance;
    for (auto& m : body.members) {
        for (auto& f : m.finits)
            if (f) rewriteAnonRefsExpr(*f, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                   &caps.fieldPaths, &caps.methodPaths);
        if (m.body) rewriteAnonRefsStmt(*m.body, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                        &caps.fieldPaths, &caps.methodPaths);
    }
    bool iface = isIfaceType(env.ctx, ne.ty);
    std::string genName = "__Anon" + std::to_string(env.counter++);
    auto g = std::make_unique<Decl>();
    g->dk = 0;
    g->mods = {"private", "sealed"};
    g->name = genName;
    TypeRef base = ne.ty;
    if (iface) g->impls.push_back(base);
    else if (!base.name.empty() && base.name != "object") g->extends = base;
    addCaptureMembers(*g, caps, env);
    g->members.push_back(buildCaptureCtor(genName, caps, std::move(ne.args), env.enclosingCls));
    std::set<std::string> capNames;
    for (auto& v : caps.capLocals) capNames.insert(v.name);
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> movedInits;
    for (auto& m : body.members) {
        if (m.mk != 0) continue;
        for (size_t fi = 0; fi < m.finits.size(); fi++) {
            if (m.finits[fi] && refsCaptureOrThis(*m.finits[fi], capNames))
                movedInits.push_back({m.names[fi], std::move(m.finits[fi])});
        }
    }
    for (auto& m : body.members) g->members.push_back(std::move(m));
    if (!movedInits.empty()) {
        for (auto& gm : g->members) {
            if (gm.mk != 2) continue;
            appendFieldInitsToCtor(gm, movedInits, capNames);
        }
    }
    Member nm;
    nm.mk = 4;
    nm.nested = std::move(g);
    nm.line = ne.line;
    env.enclosing->members.push_back(std::move(nm));
    ne.ty = TypeRef();
    ne.ty.name = genName;
    ne.anon.reset();
    for (auto& v : caps.capLocals) ne.args.push_back(mkIdent(v.name, ne.line));
    if (caps.needThis) ne.args.push_back(mkIdent("this", ne.line));
}

TypeRef resolveSemType(HEnv& env, const Expr& e);

bool samForKeptIface(Ctx& ctx, const std::string& simple, std::string& mName, TypeRef& mRet,
                     std::vector<std::pair<TypeRef, std::string>>& mParams) {
    auto it = ctx.typeByName.find(simple);
    if (it == ctx.typeByName.end()) return false;
    Decl* d = it->second;
    if (d->dk != 1) return false;
    if (ctx.samDelegates.count(simple)) return false;
    for (auto& m : d->members) {
        if (m.mk == 1 && !m.body && !hasMod(m.mods, "static")) {
            mName = m.name;
            mRet = m.ty;
            mParams = m.params;
            return true;
        }
    }
    return false;
}

void wrapSamTarget(std::unique_ptr<Expr>& slot, HEnv& env, const TypeRef& target) {
    if (!slot || target.name.empty() || target.dims != 0 || target.isVar || target.wildcard) return;
    if (slot->k != EK::Lambda && slot->k != EK::MRef) return;
    std::string mName;
    TypeRef mRet;
    std::vector<std::pair<TypeRef, std::string>> mParams;
    if (!samForKeptIface(env.ctx, firstSeg(target.name), mName, mRet, mParams)) return;
    auto ne = std::make_unique<Expr>();
    ne->k = EK::New;
    ne->line = slot->line;
    ne->ty = target;
    ne->ty.dims = 0;
    auto anon = std::make_unique<AnonBody>();
    anon->base = ne->ty;
    anon->line = slot->line;
    Member m;
    m.mk = 1;
    m.name = mName;
    m.mods.push_back("public");
    m.ty = mRet;
    m.line = slot->line;
    if (slot->k == EK::Lambda) {
        Expr& lam = *slot;
        size_t n = std::min(lam.lparams.size(), mParams.size());
        for (size_t i = 0; i < lam.lparams.size(); i++) {
            TypeRef pt = i < n ? mParams[i].first : TypeRef();
            if (pt.isVar) pt = TypeRef();
            m.params.push_back({pt, lam.lparams[i].second});
        }
        auto blk = std::make_unique<Stmt>();
        blk->k = SK::Block;
        blk->line = slot->line;
        if (lam.lbodyIsBlock) {
            blk->block = std::move(lam.lblock);
        } else if (lam.lbody) {
            auto ret = std::make_unique<Stmt>();
            ret->k = SK::Return;
            ret->line = lam.lbody->line;
            ret->e = std::move(lam.lbody);
            blk->block.push_back(std::move(ret));
        }
        m.body = std::move(blk);
    } else {
        m.params = mParams;
        Expr& mr = *slot;
        auto call = std::make_unique<Expr>();
        call->k = EK::Call;
        call->name = mr.name;
        call->line = mr.line;
        if (mr.obj) {
            std::string objName = mr.obj->k == EK::Ident ? mr.obj->name : "";
            if (mr.obj->k == EK::This || objName == "this") {
                auto th = std::make_unique<Expr>();
                th->k = EK::This;
                call->obj = std::move(th);
            } else if (!objName.empty() &&
                       (env.ctx.typeByName.count(firstSeg(objName)) || staticReceivers().count(firstSeg(objName)))) {
                auto st = std::make_unique<Expr>();
                st->k = EK::Ident;
                st->name = objName;
                call->obj = std::move(st);
            } else if (!mParams.empty()) {
                call->obj = mkIdent(mParams[0].second, mr.line);
            } else {
                call->obj = cloneExpr(*mr.obj);
            }
        }
        for (auto& p : mParams) call->args.push_back(mkIdent(p.second, mr.line));
        auto ret = std::make_unique<Stmt>();
        ret->k = SK::Return;
        ret->line = mr.line;
        ret->e = std::move(call);
        auto blk = std::make_unique<Stmt>();
        blk->k = SK::Block;
        blk->line = mr.line;
        blk->block.push_back(std::move(ret));
        m.body = std::move(blk);
    }
    anon->members.push_back(std::move(m));
    ne->anon = std::move(anon);
    processAnon(*ne, env);
    *slot = std::move(*ne);
}

TypeRef resolveSemType(HEnv& env, const Expr& e) {
    switch (e.k) {
        case EK::This: {
            TypeRef t;
            t.name = env.enclosingCls;
            return t;
        }
        case EK::Ident: {
            std::string base = firstSeg(e.name);
            if (base == "this" || base == "super") {
                TypeRef t;
                t.name = base == "this" ? env.enclosingCls : ancestorOf(env.ctx, env.enclosingCls);
                return t;
            }
            for (size_t i = env.methodVars.size(); i > 0; i--)
                if (env.methodVars[i - 1].name == base) return infoToTypeRef(env.methodVars[i - 1].ty);
            auto ft = env.ctx.classFieldType.find(env.enclosingCls + "." + base);
            if (ft != env.ctx.classFieldType.end()) return ft->second;
            std::string anc = ancestorOf(env.ctx, env.enclosingCls);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                auto ft2 = env.ctx.classFieldType.find(anc + "." + base);
                if (ft2 != env.ctx.classFieldType.end()) return ft2->second;
                anc = ancestorOf(env.ctx, anc);
            }
            if (env.ctx.enumConstOwner.count(base)) {
                TypeRef t;
                t.name = env.ctx.enumConstOwner[base];
                return t;
            }
            return TypeRef();
        }
        case EK::Field: {
            if (!e.obj) return TypeRef();
            TypeRef ot = resolveSemType(env, *e.obj);
            if (ot.name.empty()) return TypeRef();
            std::string base = firstSeg(ot.name);
            auto ft = env.ctx.classFieldType.find(base + "." + e.name);
            if (ft != env.ctx.classFieldType.end()) return ft->second;
            std::string anc = ancestorOf(env.ctx, base);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                auto ft2 = env.ctx.classFieldType.find(anc + "." + e.name);
                if (ft2 != env.ctx.classFieldType.end()) return ft2->second;
                anc = ancestorOf(env.ctx, anc);
            }
            return TypeRef();
        }
        case EK::Call: {
            std::string cls;
            if (e.obj) {
                TypeRef ot = resolveSemType(env, *e.obj);
                if (ot.name.empty()) return TypeRef();
                cls = firstSeg(ot.name);
            } else {
                cls = env.enclosingCls;
            }
            auto mit = env.ctx.methodReturns.find(cls);
            if (mit != env.ctx.methodReturns.end()) {
                auto it2 = mit->second.find(e.name + "/" + std::to_string(e.args.size()));
                if (it2 != mit->second.end()) return it2->second;
            }
            return TypeRef();
        }
        case EK::Cast:
            return e.ty;
        case EK::New:
            return e.ty;
        default:
            return TypeRef();
    }
}

std::vector<TypeRef> resolveParamTypes(HEnv& env, const Expr& call) {
    std::string cls;
    if (call.obj) {
        TypeRef ot = resolveSemType(env, *call.obj);
        if (ot.name.empty()) return {};
        cls = firstSeg(ot.name);
    } else {
        cls = env.enclosingCls;
    }
    auto it = env.ctx.methodParamTypes.find(cls + "." + call.name + "/" + std::to_string(call.args.size()));
    if (it != env.ctx.methodParamTypes.end()) return it->second;
    return {};
}

void hoistInExpr(Expr& e, HEnv& env);
std::vector<VarInfo> buildMethodVars(Ctx& ctx, Decl& cls, Member& m);

void remapLocalType(TypeRef& type, HEnv& env) {
    auto it = env.localClassMap.find(type.name);
    if (it != env.localClassMap.end()) type.name = it->second.gen;
    for (auto& arg : type.args) remapLocalType(arg, env);
}

void hoistLocalTypesInDecl(Decl& d, HEnv& env) {
    remapLocalType(d.extends, env);
    for (auto& type : d.impls) remapLocalType(type, env);
    for (auto& tp : d.tparams)
        if (tp.hasBound) remapLocalType(tp.bound, env);
    for (auto& ec : d.consts) {
        for (auto& arg : ec.args)
            if (arg) hoistInExpr(*arg, env);
        if (ec.body) hoistLocalTypesInDecl(*ec.body, env);
    }
    for (auto& m : d.members) {
        remapLocalType(m.ty, env);
        for (auto& param : m.params) remapLocalType(param.first, env);
        for (auto& type : m.throws) remapLocalType(type, env);
        for (auto& arg : m.baseArgs)
            if (arg) hoistInExpr(*arg, env);
        for (auto& init : m.finits)
            if (init) hoistInExpr(*init, env);
        if (m.body) hoistInStmt(*m.body, env);
        if (m.nested) {
            HEnv nestedEnv = env;
            for (const auto& kv : env.localClassMap) {
                if (kv.second.gen == m.nested->name) {
                    nestedEnv.currentLocalClassName = kv.first;
                    break;
                }
            }
            hoistLocalTypesInDecl(*m.nested, nestedEnv);
        }
    }
}

bool refsCaptureOrThis(const Expr& e, const std::set<std::string>& names);
bool refsCaptureOrThisStmt(const Stmt& st, const std::set<std::string>& names);

bool refsCaptureOrThis(const Expr& e, const std::set<std::string>& names) {
    if (e.k == EK::Ident) {
        std::string seg = firstSeg(e.name);
        return seg == "this" || seg == "super" || names.count(seg) > 0;
    }
    if (e.k == EK::This) return true;
    if (e.obj && refsCaptureOrThis(*e.obj, names)) return true;
    if (e.a && refsCaptureOrThis(*e.a, names)) return true;
    if (e.b && refsCaptureOrThis(*e.b, names)) return true;
    if (e.c && refsCaptureOrThis(*e.c, names)) return true;
    if (e.lbody && refsCaptureOrThis(*e.lbody, names)) return true;
    for (auto& x : e.args)
        if (x && refsCaptureOrThis(*x, names)) return true;
    for (auto& x : e.dims)
        if (x && refsCaptureOrThis(*x, names)) return true;
    if (e.lbodyIsBlock)
        for (auto& s : e.lblock)
            if (refsCaptureOrThisStmt(*s, names)) return true;
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l && refsCaptureOrThis(*l, names)) return true;
        if (arm.guard && refsCaptureOrThis(*arm.guard, names)) return true;
        if (arm.value && refsCaptureOrThis(*arm.value, names)) return true;
        for (auto& s : arm.body)
            if (refsCaptureOrThisStmt(*s, names)) return true;
    }
    return false;
}

bool refsCaptureOrThisStmt(const Stmt& st, const std::set<std::string>& names) {
    switch (st.k) {
        case SK::LocalVar:
            for (auto& x : st.inits)
                if (x && refsCaptureOrThis(*x, names)) return true;
            return false;
        case SK::Block:
            for (auto& s : st.block)
                if (refsCaptureOrThisStmt(*s, names)) return true;
            return false;
        case SK::ExprStmt:
        case SK::Return:
        case SK::Throw:
            return st.e && refsCaptureOrThis(*st.e, names);
        case SK::If:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            if (st.body && refsCaptureOrThisStmt(*st.body, names)) return true;
            if (st.body2 && refsCaptureOrThisStmt(*st.body2, names)) return true;
            return false;
        case SK::While:
        case SK::DoWhile:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::For:
            for (auto& x : st.inits)
                if (x && refsCaptureOrThis(*x, names)) return true;
            for (auto& x : st.initExprs)
                if (x && refsCaptureOrThis(*x, names)) return true;
            for (auto& x : st.updateExprs)
                if (x && refsCaptureOrThis(*x, names)) return true;
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::Foreach:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::Try:
            for (auto& x : st.resInits)
                if (x && refsCaptureOrThis(*x, names)) return true;
            if (st.body && refsCaptureOrThisStmt(*st.body, names)) return true;
            for (auto& c : st.catches)
                if (refsCaptureOrThisStmt(*c.body, names)) return true;
            return st.fin && refsCaptureOrThisStmt(*st.fin, names);
        case SK::Switch:
            if (st.subject && refsCaptureOrThis(*st.subject, names)) return true;
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l && refsCaptureOrThis(*l, names)) return true;
                if (arm.guard && refsCaptureOrThis(*arm.guard, names)) return true;
                if (arm.value && refsCaptureOrThis(*arm.value, names)) return true;
                for (auto& s : arm.body)
                    if (refsCaptureOrThisStmt(*s, names)) return true;
            }
            return false;
        case SK::Sync:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        case SK::Assert:
            if (st.e && refsCaptureOrThis(*st.e, names)) return true;
            return st.e2 && refsCaptureOrThis(*st.e2, names);
        case SK::Label:
            return st.body && refsCaptureOrThisStmt(*st.body, names);
        default:
            return false;
    }
}

std::unique_ptr<Stmt> makeFieldAssign(const std::string& name, std::unique_ptr<Expr> value) {
    auto as = std::make_unique<Stmt>();
    as->k = SK::ExprStmt;
    auto tgt = std::make_unique<Expr>();
    tgt->k = EK::Field;
    tgt->name = name;
    tgt->obj = mkIdent("this");
    auto assign = std::make_unique<Expr>();
    assign->k = EK::Assign;
    assign->op = "=";
    assign->a = std::move(tgt);
    assign->b = std::move(value);
    as->e = std::move(assign);
    return as;
}

void appendFieldInitsToCtor(Member& ctor, const std::vector<std::pair<std::string, std::unique_ptr<Expr>>>& inits,
                            const std::set<std::string>& capNames) {
    if (!ctor.body || ctor.body->k != SK::Block) return;
    size_t pos = 0;
    if (!ctor.body->block.empty()) {
        const Stmt& first = *ctor.body->block.front();
        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) pos = 1;
    }
    while (pos < ctor.body->block.size()) {
        const Stmt& s = *ctor.body->block[pos];
        if (s.k == SK::ExprStmt && s.e && s.e->k == EK::Assign) {
            const Expr& tgt = *s.e->a;
            const Expr& val = *s.e->b;
            if (tgt.k == EK::Field && tgt.obj && tgt.obj->k == EK::Ident && tgt.obj->name == "this" &&
                val.k == EK::Ident && val.name == tgt.name &&
                (tgt.name == "__this" || capNames.count(tgt.name) > 0)) {
                pos++;
                continue;
            }
        }
        break;
    }
    for (auto it = inits.rbegin(); it != inits.rend(); ++it) {
        ctor.body->block.insert(ctor.body->block.begin() + static_cast<std::ptrdiff_t>(pos),
                                makeFieldAssign(it->first, cloneExpr(*it->second)));
    }
}

void processLocalClass(Stmt& st, HEnv& env) {
    if (!st.lclass) {
        st.k = SK::Empty;
        return;
    }
    Decl& d = *st.lclass;
    std::string origName = d.name;
    std::string genName = "__LC" + std::to_string(env.counter++) + "_" + origName;
    HEnv bodyEnv = env;
    bodyEnv.enclosing = &d;
    bodyEnv.enclosingCls = genName;
    bodyEnv.currentLocalClassName = origName;
    bodyEnv.hasEnclosingInstance = true;
    bodyEnv.outerFieldPaths.clear();
    bodyEnv.outerMethodPaths.clear();
    if (env.hasEnclosingInstance) {
        for (const auto& entry : env.outerFieldPaths)
            bodyEnv.outerFieldPaths[entry.first] = "__this" + (entry.second.empty() ? "" : "." + entry.second);
        for (const auto& entry : env.outerMethodPaths)
            bodyEnv.outerMethodPaths[entry.first] = "__this" + (entry.second.empty() ? "" : "." + entry.second);
    }
    for (const auto& member : d.members) {
        if (member.mk == 0 && !hasMod(member.mods, "static")) {
            for (const auto& name : member.names) bodyEnv.outerFieldPaths[name] = "";
        } else if (member.mk == 1 && !hasMod(member.mods, "static")) {
            bodyEnv.outerMethodPaths[member.name] = "";
        }
    }
    const size_t originalMemberCount = d.members.size();
    for (size_t index = 0; index < originalMemberCount; ++index) {
        int memberKind = d.members[index].mk;
        bool isStatic = hasMod(d.members[index].mods, "static") || d.members[index].isStaticInit;
        Stmt* body = d.members[index].body.get();
        std::vector<Expr*> initializers;
        for (auto& initializer : d.members[index].finits)
            if (initializer) initializers.push_back(initializer.get());
        HEnv memberEnv = bodyEnv;
        if (memberKind == 1 || memberKind == 2) {
            memberEnv.hasEnclosingInstance = !isStatic;
            if (memberKind == 1 && !d.members[index].ty.empty() && !d.members[index].ty.isVar) {
                memberEnv.hasRet = true;
                memberEnv.methodRet = d.members[index].ty;
            }
            std::vector<VarInfo> vars = buildMethodVars(env.ctx, d, d.members[index]);
            memberEnv.methodVars.insert(memberEnv.methodVars.end(), vars.begin(), vars.end());
        } else if (memberKind == 0 || memberKind == 3) {
            memberEnv.hasEnclosingInstance = !isStatic;
        }
        for (Expr* initializer : initializers) {
            if (initializer) hoistInExpr(*initializer, memberEnv);
        }
        if (body) hoistInStmt(*body, memberEnv);
        bodyEnv.counter = std::max(bodyEnv.counter, memberEnv.counter);
        for (auto& entry : memberEnv.localClassMap) bodyEnv.localClassMap[entry.first] = entry.second;
    }
    env.counter = std::max(env.counter, bodyEnv.counter);
    std::set<std::string> typeNames;
    for (auto& kv : env.ctx.typeByName) typeNames.insert(kv.first);
    std::set<std::string> declared, used;
    bool needThisRef = false;
    for (size_t index = 0; index < originalMemberCount; ++index) {
        const Member& member = d.members[index];
        for (auto& name : member.names) declared.insert(name);
        if (member.mk == 1 || member.mk == 2) {
            if (member.mk == 1) declared.insert(member.name);
            for (auto& param : member.params) declared.insert(param.second);
            for (auto& tp : member.tparams) declared.insert(tp.name);
            if (member.body) {
                std::vector<VarInfo> locals;
                collectLocalsStmt(*member.body, env.ctx, locals);
                for (auto& local : locals) declared.insert(local.name);
            }
        }
        for (auto& initializer : member.finits)
            if (initializer) walkIdentsExpr(*initializer, typeNames, declared, used, &needThisRef);
        if (member.body) walkIdentsStmt(*member.body, typeNames, declared, used, &needThisRef);
    }
    CapResult caps = computeCaptures(env, declared, used, needThisRef);
    caps.needThis = caps.needThis || env.hasEnclosingInstance;
    const LocalClassInfo* localBase = nullptr;
    auto baseIt = env.localClassMap.find(d.extends.name);
    if (baseIt != env.localClassMap.end()) {
        localBase = &baseIt->second;
        for (const auto& captured : localBase->caps) {
            auto found = std::find_if(caps.capLocals.begin(), caps.capLocals.end(),
                                      [&](const VarInfo& value) { return value.name == captured.name; });
            if (found == caps.capLocals.end()) caps.capLocals.push_back(captured);
        }
        caps.needThis = caps.needThis || localBase->needThis;
    }
    std::vector<std::unique_ptr<Expr>> localBaseArgs;
    if (localBase) {
        for (const auto& captured : localBase->caps)
            localBaseArgs.push_back(mkIdent(captured.name, st.line));
        if (localBase->needThis) localBaseArgs.push_back(mkIdent("__this", st.line));
    }
    for (size_t index = 0; index < originalMemberCount; ++index) {
        Member& member = d.members[index];
        for (auto& initializer : member.finits) {
            if (initializer)
                rewriteAnonRefsExpr(*initializer, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                    &caps.fieldPaths, &caps.methodPaths);
        }
        if (member.body)
            rewriteAnonRefsStmt(*member.body, caps.capFieldNames, caps.capMethodNames, caps.needThis,
                                &caps.fieldPaths, &caps.methodPaths);
    }
    bool hasCtor = false;
    for (size_t index = 0; index < originalMemberCount; ++index)
        if (d.members[index].mk == 2) hasCtor = true;
    if (!caps.capLocals.empty() || caps.needThis || !hasCtor) {
        for (size_t index = 0; index < originalMemberCount; ++index) {
            Member& member = d.members[index];
            if (member.mk != 2) continue;
            for (auto& variable : caps.capLocals)
                member.params.push_back({infoToTypeRef(variable.ty), variable.name});
            if (caps.needThis) {
                TypeRef outerType;
                outerType.name = env.enclosingCls;
                member.params.push_back({outerType, "__this"});
            }
            bool hasExplicitSuperCall = false;
            if (member.body && member.body->k == SK::Block && !member.body->block.empty()) {
                Stmt& first = *member.body->block.front();
                if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) {
                    Expr& ctorCall = *first.e;
                    if (ctorCall.name == "super") {
                        hasExplicitSuperCall = true;
                        for (const auto& baseArg : localBaseArgs) {
                            auto copy = std::make_unique<Expr>();
                            copy->k = baseArg->k;
                            copy->name = baseArg->name;
                            copy->line = baseArg->line;
                            ctorCall.args.push_back(std::move(copy));
                        }
                    } else if (ctorCall.name == "this") {
                        for (const auto& captured : caps.capLocals)
                            ctorCall.args.push_back(mkIdent(captured.name, st.line));
                        if (caps.needThis) ctorCall.args.push_back(mkIdent("__this", st.line));
                    }
                }
            }
            if (!hasExplicitSuperCall) {
                for (const auto& baseArg : localBaseArgs) {
                    auto copy = std::make_unique<Expr>();
                    copy->k = baseArg->k;
                    copy->name = baseArg->name;
                    copy->line = baseArg->line;
                    member.baseArgs.push_back(std::move(copy));
                }
            }
            if (member.body && member.body->k == SK::Block) {
                for (auto variable = caps.capLocals.rbegin(); variable != caps.capLocals.rend(); ++variable) {
                    auto assignment = std::make_unique<Stmt>();
                    assignment->k = SK::ExprStmt;
                    auto target = std::make_unique<Expr>();
                    target->k = EK::Field;
                    target->name = variable->name;
                    target->obj = mkIdent("this");
                    auto assignExpr = std::make_unique<Expr>();
                    assignExpr->k = EK::Assign;
                    assignExpr->op = "=";
                    assignExpr->a = std::move(target);
                    assignExpr->b = mkIdent(variable->name);
                    assignment->e = std::move(assignExpr);
                    size_t insertion = 0;
                    if (!member.body->block.empty()) {
                        const Stmt& first = *member.body->block.front();
                        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) insertion = 1;
                    }
                    member.body->block.insert(member.body->block.begin() + static_cast<std::ptrdiff_t>(insertion),
                                              std::move(assignment));
                }
                if (caps.needThis) {
                    auto assignment = std::make_unique<Stmt>();
                    assignment->k = SK::ExprStmt;
                    auto target = std::make_unique<Expr>();
                    target->k = EK::Field;
                    target->name = "__this";
                    target->obj = mkIdent("this");
                    auto assignExpr = std::make_unique<Expr>();
                    assignExpr->k = EK::Assign;
                    assignExpr->op = "=";
                    assignExpr->a = std::move(target);
                    assignExpr->b = mkIdent("__this");
                    assignment->e = std::move(assignExpr);
                    size_t insertion = 0;
                    if (!member.body->block.empty()) {
                        const Stmt& first = *member.body->block.front();
                        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) insertion = 1;
                    }
                    member.body->block.insert(member.body->block.begin() + static_cast<std::ptrdiff_t>(insertion),
                                              std::move(assignment));
                }
            }
        }
        if (!hasCtor) {
            Member constructor = buildCaptureCtor(d.name, caps, std::move(localBaseArgs), env.enclosingCls);
            d.members.insert(d.members.begin(), std::move(constructor));
        }
        if (caps.needThis) {
            Member outerField;
            outerField.mk = 0;
            outerField.mods = {"private", "readonly"};
            outerField.ty.name = env.enclosingCls;
            outerField.names.push_back("__this");
            outerField.fdims.push_back(0);
            outerField.finits.push_back(nullptr);
            d.members.insert(d.members.begin(), std::move(outerField));
        }
        for (auto variable = caps.capLocals.rbegin(); variable != caps.capLocals.rend(); ++variable) {
            Member capturedField;
            capturedField.mk = 0;
            capturedField.mods = {"private", "readonly"};
            capturedField.ty = infoToTypeRef(variable->ty);
            capturedField.names.push_back(variable->name);
            capturedField.fdims.push_back(0);
            capturedField.finits.push_back(nullptr);
            d.members.insert(d.members.begin(), std::move(capturedField));
        }
    }
    std::set<std::string> capNames;
    for (auto& v : caps.capLocals) capNames.insert(v.name);
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> movedInits;
    for (auto& member : d.members) {
        if (member.mk != 0) continue;
        for (size_t fi = 0; fi < member.finits.size(); fi++) {
            if (member.finits[fi] && refsCaptureOrThis(*member.finits[fi], capNames))
                movedInits.push_back({member.names[fi], std::move(member.finits[fi])});
        }
    }
    if (!movedInits.empty()) {
        for (auto& member : d.members) {
            if (member.mk != 2) continue;
            appendFieldInitsToCtor(member, movedInits, capNames);
        }
    }
    LocalClassInfo lci;
    lci.orig = origName;
    lci.gen = genName;
    lci.enclosingCls = env.enclosingCls;
    lci.caps = caps.capLocals;
    lci.needThis = caps.needThis;
    env.localClassMap[origName] = lci;
    bodyEnv.localClassMap[origName] = lci;
    bodyEnv.currentLocalClassName = origName;
    hoistLocalTypesInDecl(d, bodyEnv);
    d.mods.insert(d.mods.begin(), "private");
    d.mods.push_back("sealed");
    d.name = genName;
    Member nestedMember;
    nestedMember.mk = 4;
    nestedMember.nested = std::move(st.lclass);
    nestedMember.line = st.line;
    env.enclosing->members.push_back(std::move(nestedMember));
    st.k = SK::Empty;
}

void hoistInExpr(Expr& e, HEnv& env) {
    if (e.k == EK::Cast) wrapSamTarget(e.a, env, e.ty);
    if (e.k == EK::Call) {
        std::vector<TypeRef> ptypes = resolveParamTypes(env, e);
        for (size_t i = 0; i < ptypes.size() && i < e.args.size(); i++)
            wrapSamTarget(e.args[i], env, ptypes[i]);
    }
    if (e.k == EK::New && !e.anon) {
        auto pit = env.ctx.methodParamTypes.find(firstSeg(e.ty.name) + ".<init>/" +
                                                 std::to_string(e.args.size()));
        for (size_t i = 0; pit != env.ctx.methodParamTypes.end() && i < pit->second.size() &&
                           i < e.args.size();
             i++)
            wrapSamTarget(e.args[i], env, pit->second[i]);
    }
    if (e.k == EK::New && !e.anon) {
        auto it = env.localClassMap.find(e.ty.name);
        if (it != env.localClassMap.end()) {
            e.ty.name = it->second.gen;
            for (auto& v : it->second.caps) e.args.push_back(mkIdent(v.name, e.line));
            if (it->second.needThis) {
                std::unique_ptr<Expr> outerThis = mkIdent("this", e.line);
                std::string current = env.currentLocalClassName;
                std::set<std::string> visited;
                while (!current.empty() && visited.insert(current).second) {
                    auto currentInfo = env.localClassMap.find(current);
                    if (currentInfo == env.localClassMap.end()) break;
                    if (currentInfo->second.gen == it->second.enclosingCls) break;
                    if (!currentInfo->second.needThis) break;
                    auto field = std::make_unique<Expr>();
                    field->k = EK::Field;
                    field->name = "__this";
                    field->obj = std::move(outerThis);
                    outerThis = std::move(field);
                    if (currentInfo->second.enclosingCls == it->second.enclosingCls) break;
                    auto parentInfo = std::find_if(
                        env.localClassMap.begin(), env.localClassMap.end(),
                        [&](const auto& entry) { return entry.second.gen == currentInfo->second.enclosingCls; });
                    if (parentInfo == env.localClassMap.end()) break;
                    current = parentInfo->first;
                }
                e.args.push_back(std::move(outerThis));
            }
        } else {
            remapLocalType(e.ty, env);
        }
    } else {
        remapLocalType(e.ty, env);
    }
    for (auto& type : e.targs) remapLocalType(type, env);
    for (auto& param : e.lparams) remapLocalType(param.first, env);
    for (auto& type : e.patComps) remapLocalType(type.first, env);
    if (e.anon) remapLocalType(e.anon->base, env);
    if (e.obj) hoistInExpr(*e.obj, env);
    if (e.a) hoistInExpr(*e.a, env);
    if (e.b) hoistInExpr(*e.b, env);
    if (e.c) hoistInExpr(*e.c, env);
    if (e.lbody) hoistInExpr(*e.lbody, env);
    for (auto& x : e.args)
        if (x) hoistInExpr(*x, env);
    for (auto& x : e.dims)
        if (x) hoistInExpr(*x, env);
    if (e.lbodyIsBlock) {
        bool savedHas = env.hasRet;
        TypeRef savedRet = env.methodRet;
        env.hasRet = false;
        env.methodRet = TypeRef();
        for (auto& s : e.lblock) hoistInStmt(*s, env);
        env.hasRet = savedHas;
        env.methodRet = savedRet;
    }
    for (auto& arm : e.cases) {
        for (auto& l : arm.labels)
            if (l) hoistInExpr(*l, env);
        if (arm.guard) hoistInExpr(*arm.guard, env);
        if (arm.value) hoistInExpr(*arm.value, env);
        for (auto& s : arm.body) hoistInStmt(*s, env);
    }
    if (e.k == EK::New && e.anon) processAnon(e, env);
}

void hoistInStmt(Stmt& st, HEnv& env) {
    remapLocalType(st.ty, env);
    for (auto& resource : st.resources) remapLocalType(resource.first, env);
    for (auto& c : st.catches)
        for (auto& type : c.types) remapLocalType(type, env);
    switch (st.k) {
        case SK::LocalClass:
            processLocalClass(st, env);
            return;
        case SK::Block:
            for (auto& s : st.block) hoistInStmt(*s, env);
            break;
        case SK::ExprStmt:
            if (st.e && st.e->k == EK::Assign && st.e->op == "=") {
                TypeRef lhsTy = resolveSemType(env, *st.e->a);
                wrapSamTarget(st.e->b, env, lhsTy);
            }
            if (st.e) hoistInExpr(*st.e, env);
            break;
        case SK::LocalVar:
            if (!st.ty.isVar)
                for (auto& x : st.inits)
                    if (x) wrapSamTarget(x, env, st.ty);
            for (auto& x : st.inits)
                if (x) hoistInExpr(*x, env);
            break;
        case SK::If:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            if (st.body2) hoistInStmt(*st.body2, env);
            break;
        case SK::While:
        case SK::DoWhile:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::For:
            if (!st.ty.isVar)
                for (auto& x : st.inits)
                    if (x) wrapSamTarget(x, env, st.ty);
            for (auto& x : st.initExprs)
                if (x) {
                    if (x->k == EK::Assign && x->op == "=") {
                        TypeRef lhsTy = resolveSemType(env, *x->a);
                        wrapSamTarget(x->b, env, lhsTy);
                    }
                    hoistInExpr(*x, env);
                }
            for (auto& x : st.updateExprs)
                if (x) hoistInExpr(*x, env);
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::Foreach:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::Return:
        case SK::Throw:
            if (st.e && st.k == SK::Return && env.hasRet) wrapSamTarget(st.e, env, env.methodRet);
            if (st.e) hoistInExpr(*st.e, env);
            break;
        case SK::Try:
            for (auto& x : st.resInits)
                if (x) hoistInExpr(*x, env);
            if (st.body) hoistInStmt(*st.body, env);
            for (auto& c : st.catches) hoistInStmt(*c.body, env);
            if (st.fin) hoistInStmt(*st.fin, env);
            break;
        case SK::Switch:
            if (st.subject) hoistInExpr(*st.subject, env);
            for (auto& arm : st.arms) {
                for (auto& l : arm.labels)
                    if (l) hoistInExpr(*l, env);
                if (arm.guard) hoistInExpr(*arm.guard, env);
                if (arm.value) hoistInExpr(*arm.value, env);
                for (auto& s : arm.body) hoistInStmt(*s, env);
            }
            break;
        case SK::Sync:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.body) hoistInStmt(*st.body, env);
            break;
        case SK::Assert:
            if (st.e) hoistInExpr(*st.e, env);
            if (st.e2) hoistInExpr(*st.e2, env);
            break;
        case SK::Label:
            if (st.body) hoistInStmt(*st.body, env);
            break;
        default:
            break;
    }
}

std::vector<VarInfo> buildMethodVars(Ctx& ctx, Decl& cls, Member& m) {
    (void)cls;
    std::vector<VarInfo> out;
    for (auto& p : m.params) out.push_back({p.second, toInfo(ctx, p.first)});
    if (m.body) collectLocalsStmt(*m.body, ctx, out);
    return out;
}

std::set<std::string> outerMemberNames(Ctx& ctx, const std::string& cls) {
    std::set<std::string> out = chainInstanceFields(ctx, cls);
    std::set<std::string> ms = chainInstanceMethods(ctx, cls);
    out.insert(ms.begin(), ms.end());
    return out;
}

void hoistInDecl(Ctx& ctx, Decl& cls) {
    size_t processed = 0;
    while (processed < cls.members.size()) {
        Member& m = cls.members[processed];
        int mk = m.mk;
        Stmt* body = m.body.get();
        Decl* nested = m.nested.get();
        std::vector<Expr*> finits;
        for (auto& f : m.finits)
            if (f) finits.push_back(f.get());
        processed++;
        if ((mk == 1 || mk == 2) && body) {
            HEnv env(ctx, &cls, cls.name);
            env.methodVars = buildMethodVars(ctx, cls, m);
            env.outerMembers = outerMemberNames(ctx, cls.name);
            if (mk == 1 && !m.ty.empty() && !m.ty.isVar) {
                env.hasRet = true;
                env.methodRet = m.ty;
            }
            env.hasEnclosingInstance = !hasMod(m.mods, "static");
            for (const auto& field : chainInstanceFields(ctx, cls.name)) env.outerFieldPaths[field] = "";
            for (const auto& method : chainInstanceMethods(ctx, cls.name)) env.outerMethodPaths[method] = "";
            hoistInStmt(*body, env);
        }
        for (Expr* f : finits) {
            HEnv env(ctx, &cls, cls.name);
            env.outerMembers = outerMemberNames(ctx, cls.name);
            env.hasEnclosingInstance = !hasMod(m.mods, "static");
            for (const auto& field : chainInstanceFields(ctx, cls.name)) env.outerFieldPaths[field] = "";
            for (const auto& method : chainInstanceMethods(ctx, cls.name)) env.outerMethodPaths[method] = "";
            hoistInExpr(*f, env);
        }
        if (mk == 4 && nested) hoistInDecl(ctx, *nested);
        if (mk == 3 && body) {
            HEnv env(ctx, &cls, cls.name);
            env.outerMembers = outerMemberNames(ctx, cls.name);
            env.hasEnclosingInstance = !m.isStaticInit;
            for (const auto& field : chainInstanceFields(ctx, cls.name)) env.outerFieldPaths[field] = "";
            for (const auto& method : chainInstanceMethods(ctx, cls.name)) env.outerMethodPaths[method] = "";
            hoistInStmt(*body, env);
        }
    }
}

void hoistUnit(Ctx& ctx, Unit& u) {
    for (auto& d : u.types) hoistInDecl(ctx, *d);
    for (auto& d : u.types) {
        for (auto& ec : d->consts) {
            if (!ec.body) continue;
            Decl& eb = *ec.body;
            size_t processed = 0;
            while (processed < eb.members.size()) {
                Member& m = eb.members[processed];
                Stmt* body = m.body.get();
                processed++;
                if (body) {
                    HEnv env(ctx, &eb, eb.name);
                    env.outerMembers = outerMemberNames(ctx, eb.name);
                    hoistInStmt(*body, env);
                }
            }
        }
    }
}


std::unique_ptr<Expr> cloneExpr(const Expr& e) {
    auto c = std::make_unique<Expr>();
    c->k = e.k;
    c->op = e.op;
    c->name = e.name;
    c->val = e.val;
    c->lt = e.lt;
    c->rawLit = e.rawLit;
    c->postfix = e.postfix;
    c->diamond = e.diamond;
    c->implArr = e.implArr;
    c->ty = e.ty;
    c->lbodyIsBlock = e.lbodyIsBlock;
    c->patName = e.patName;
    c->patComps = e.patComps;
    c->targs = e.targs;
    c->line = e.line;
    if (e.obj) c->obj = cloneExpr(*e.obj);
    if (e.a) c->a = cloneExpr(*e.a);
    if (e.b) c->b = cloneExpr(*e.b);
    if (e.c) c->c = cloneExpr(*e.c);
    if (e.lbody) c->lbody = cloneExpr(*e.lbody);
    for (auto& x : e.args)
        if (x) c->args.push_back(cloneExpr(*x));
    for (auto& x : e.dims)
        if (x) c->dims.push_back(cloneExpr(*x));
    for (auto& p : e.lparams) c->lparams.push_back(p);
    return c;
}

void substParamExpr(Expr& e, const std::string& param, const Expr& rep) {
    if (e.k == EK::Ident && e.name == param) {
        std::unique_ptr<Expr> cloned = cloneExpr(rep);
        e = std::move(*cloned);
        return;
    }
    if (e.obj) substParamExpr(*e.obj, param, rep);
    if (e.a) substParamExpr(*e.a, param, rep);
    if (e.b) substParamExpr(*e.b, param, rep);
    if (e.c) substParamExpr(*e.c, param, rep);
    if (e.lbody) substParamExpr(*e.lbody, param, rep);
    for (auto& x : e.args)
        if (x) substParamExpr(*x, param, rep);
    for (auto& x : e.dims)
        if (x) substParamExpr(*x, param, rep);
}
struct ES {
    Ctx& ctx;
    std::ostringstream out;
    std::ostream* cur;
    int indent = 0;
    std::string cls;
    Decl* decl = nullptr;
    std::vector<VarInfo> scope;
    std::set<std::string> activeLabels;
    std::set<std::string> typeParams;
    int tmpCounter = 0;
    bool inCtor = false;
    bool isInterfaceCtx = false;
    std::string switchSubjectType;
    TypeRef curRet;
    bool hasCurRet = false;

    explicit ES(Ctx& c) : ctx(c), cur(&out) {}

    void w(const std::string& s) { (*cur) << s; }
    void wi() {
        for (int i = 0; i < indent; i++) (*cur) << "    ";
    }
    void nl() {
        (*cur) << "\n";
    }
    void line(const std::string& s) {
        wi();
        w(s);
        nl();
    }
};

void emitStmt(ES& es, Stmt& st);
void emitExpr(ES& es, Expr& e, int prec = 0, const TypeInfo& expect = TypeInfo());
TypeInfo inferExprType(ES& es, Expr& e);
void flushComments(ES& es, int line);
void flushTrailing(ES& es, int line);
void emitType(ES& es, Decl& d);
void emitBodyBlock(ES& es, Stmt& body);
std::string delegateSigFor(Ctx& ctx, Decl& iface);
std::string captureExprText(ES& es, Expr& e, int prec = 0, const TypeInfo& expect = TypeInfo());

std::string pascalCase(const std::string& s);

void emitAnnotations(ES& es, std::vector<Ann>& anns) {
    for (auto& a : anns) {
        if (a.name == "Override" || a.name == "java.lang.Override" || a.name == "SuppressWarnings" ||
            a.name == "java.lang.SuppressWarnings" || a.name == "FunctionalInterface" ||
            a.name == "SafeVarargs" || a.name == "java.lang.SafeVarargs" ||
            a.name == "java.lang.FunctionalInterface")
            continue;
        flushComments(es, a.line);
        std::string mapped = a.name;
        size_t dot = mapped.rfind('.');
        if (dot != std::string::npos) {
            std::string simple = mapped.substr(dot + 1);
            if (startsWith(mapped, "java.lang.annotation") || startsWith(mapped, "java.lang.annotation.")) continue;
            if (startsWith(mapped, "java.lang")) mapped = simple;
            else if (startsWith(mapped, "java.")) continue;
        }
        if (mapped == "Deprecated") {
            es.wi();
            es.line("[Obsolete]");
            continue;
        }
        std::string s = "[" + mapped + "(";
        bool first = true;
        for (auto& pa : a.pargs) {
            if (!first) s += ", ";
            first = false;
            s += captureExprText(es, *pa);
        }
        for (auto& kv : a.args) {
            if (!first) s += ", ";
            first = false;
            s += pascalCase(kv.first) + " = " + captureExprText(es, *kv.second);
        }
        es.wi();
        es.w(s);
        es.w(")]");
        es.nl();
    }
}

std::string pascalCase(const std::string& s) {
    if (s.empty()) return s;
    std::string r = s;
    r[0] = (char)std::toupper((unsigned char)r[0]);
    return r;
}

std::string csTypeStr(Ctx& ctx, const TypeRef& t, bool genArg = false) {
    if (t.isVar) return "var";
    if (t.wildcard) {
        if (t.name.empty()) return "object";
        TypeRef b;
        b.name = t.name;
        b.args = t.args;
        return csTypeStr(ctx, b, genArg);
    }
    if (t.name.empty()) return "object";
    if (t.name == "Optional" && t.args.size() == 1) {
        return csTypeStr(ctx, t.args[0], true) + "?";
    }
    std::string mapped = mapJavaTypeFqn(t.name, genArg);
    if (mapped.empty()) mapped = t.name;
    if (mapped == "__Scanner") ctx.needScanner = true;
    if (mapped == "StringBuilder") ctx.needText = true;
    if (mapped == "Thread") ctx.needThreading = true;
    std::string r = mapped;
    if (!t.args.empty()) {
        r += "<";
        for (size_t i = 0; i < t.args.size(); i++) {
            if (i) r += ", ";
            r += csTypeStr(ctx, t.args[i], true);
        }
        r += ">";
    }
    for (int i = 0; i < t.dims; i++) r += "[]";
    return r;
}

std::string visOf(const std::vector<std::string>& mods) {
    if (hasMod(mods, "public")) return "public";
    if (hasMod(mods, "private")) return "private";
    if (hasMod(mods, "protected")) return "protected";
    return "internal";
}

std::string tparamsStr(ES& es, const std::vector<TP>& tps, bool* hasWhere) {
    std::string r;
    std::string where;
    if (!tps.empty()) {
        r += "<";
        for (size_t i = 0; i < tps.size(); i++) {
            if (i) r += ", ";
            r += tps[i].name;
        }
        r += ">";
        for (auto& tp : tps) {
            if (tp.hasBound && !tp.bound.empty()) {
                where += " where " + tp.name + " : " + csTypeStr(es.ctx, tp.bound);
            }
        }
    }
    if (hasWhere) *hasWhere = !where.empty();
    return r + where;
}

bool isConstAble(ES& es, Member& m) {
    std::string t = csTypeStr(es.ctx, m.ty);
    if (t != "int" && t != "long" && t != "double" && t != "float" && t != "bool" &&
        t != "string" && t != "char" && t != "byte" && t != "short" && t != "decimal")
        return false;
    for (auto& f : m.finits) {
        if (!f) return false;
        if (f->k != EK::Lit && !(f->k == EK::Un && f->a && f->a->k == EK::Lit)) return false;
    }
    return true;
}

void emitField(ES& es, Member& m) {
    flushComments(es, m.line);
    emitAnnotations(es, m.anns);
    std::string vis = visOf(m.mods);
    bool stat = hasMod(m.mods, "static");
    bool fin = hasMod(m.mods, "final");
    bool vol = hasMod(m.mods, "volatile");
    std::string base = csTypeStr(es.ctx, m.ty);
    bool constAble = fin && stat && isConstAble(es, m);
    for (size_t i = 0; i < m.names.size(); i++) {
        std::string t = base;
        for (int k = 0; k < m.fdims[i]; k++) t += "[]";
        std::string mods = vis + " ";
        if (constAble) mods += "const ";
        else {
            if (stat) mods += "static ";
            if (fin) mods += "readonly ";
        }
        if (vol && t != "long" && t != "double" && t != "float") mods += "volatile ";
        flushComments(es, m.line);
        es.wi();
        es.w(mods + t + " " + csIdent(m.names[i]));
        if (i < m.finits.size() && m.finits[i]) {
            es.w(" = ");
            TypeInfo expect = toInfo(es.ctx, m.ty);
            expect.dims += m.fdims[i];
            expect.known = !m.ty.name.empty();
            emitExpr(es, *m.finits[i], 0, expect);
        }
        es.w(";");
        flushTrailing(es, m.line);
        es.nl();
    }
}

void emitInitBlock(ES& es, Member& m, Decl& d) {
    if (m.isStaticInit) {
        flushComments(es, m.line);
        es.wi();
        es.w("static " + d.name + "()");
        es.nl();
        es.line("{");
        es.indent++;
        emitBodyBlock(es, *m.body);
        es.indent--;
        es.line("}");
    } else {
        es.wi();
        es.w("private void __InstanceInit()");
        es.nl();
        es.line("{");
        es.indent++;
        emitBodyBlock(es, *m.body);
        es.indent--;
        es.line("}");
        es.nl();
    }
}

std::string mapMethodName(ES& es, const std::string& recvType, const std::string& name) {
    (void)es;
    (void)recvType;
    return name;
}

std::string paramListStr(ES& es, Member& m) {
    std::string r;
    for (size_t i = 0; i < m.params.size(); i++) {
        if (i) r += ", ";
        if (m.varargs && i == m.params.size() - 1) {
            TypeRef at = m.params[i].first;
            if (at.dims == 0) at.dims = 1;
            r += "params " + csTypeStr(es.ctx, at) + " " + csIdent(m.params[i].second);
        } else {
            r += csTypeStr(es.ctx, m.params[i].first) + " " + csIdent(m.params[i].second);
        }
    }
    return r;
}

void emitMethodCommon(ES& es, Decl& d, Member& m, const std::string& name,
                      const std::string& modsPrefix, bool emitBody) {
    (void)d;
    flushComments(es, m.line);
    emitAnnotations(es, m.anns);
    bool hasWhere = false;
    std::string tp = tparamsStr(es, m.tparams, &hasWhere);
    std::string ret = csTypeStr(es.ctx, m.ty);
    std::set<std::string> savedTypeParams = es.typeParams;
    for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
    es.wi();
    std::string sig = modsPrefix;
    sig += ret + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")";
    es.w(sig);
    flushTrailing(es, m.line);
    es.nl();
    if (!emitBody || !m.body) {
        es.wi();
        es.w(";");
        es.nl();
        es.typeParams = savedTypeParams;
        return;
    }
    es.line("{");
    es.indent++;
    TypeRef savedRet = es.curRet;
    bool savedHas = es.hasCurRet;
    es.curRet = m.ty;
    es.hasCurRet = true;
    std::vector<VarInfo> pushed;
    for (size_t i = 0; i < m.params.size(); i++) {
        VarInfo vi;
        vi.name = m.params[i].second;
        vi.ty = toInfo(es.ctx, m.params[i].first);
        es.scope.push_back(vi);
        pushed.push_back(vi);
    }
    emitBodyBlock(es, *m.body);
    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
    es.curRet = savedRet;
    es.hasCurRet = savedHas;
    es.typeParams = savedTypeParams;
    es.indent--;
    es.line("}");
}

std::string methodRename(ES& es, Decl& d, Member& m, bool* isOverride, bool* isVirtual) {
    *isOverride = false;
    *isVirtual = false;
    std::string key = m.name + "/" + std::to_string(m.params.size());
    if (m.name == "main" && hasMod(m.mods, "static")) {
        if (es.ctx.mainCount == 1) return "Main";
        return "Main_" + d.name;
    }
    if (hasMod(m.mods, "static")) return m.name;
    if (m.name == "toString" && m.params.empty()) {
        *isOverride = true;
        return "ToString";
    }
    if (m.name == "hashCode" && m.params.empty()) {
        *isOverride = true;
        return "GetHashCode";
    }
    if (m.name == "equals" && m.params.size() == 1 && csTypeStr(es.ctx, m.params[0].first) == "object") {
        *isOverride = true;
        return "Equals";
    }
    if (m.name == "compareTo" && m.params.size() == 1) {
        return "CompareTo";
    }
    if (m.name == "main" && hasMod(m.mods, "static")) {
        if (es.ctx.mainCount == 1) return "Main";
        return "Main_" + d.name;
    }
    if (m.name == "iterator" && m.params.empty()) {
        bool iter = false;
        for (auto& imp : d.impls)
            if (firstSeg(imp.name) == "Iterable") iter = true;
        if (iter) return "GetEnumerator";
    }
    auto& ov = es.ctx.overrideM[d.name];
    if (ov.count(key)) {
        *isOverride = true;
        return m.name;
    }
    auto& vt = es.ctx.virtualM[d.name];
    if (vt.count(key)) *isVirtual = true;
    return m.name;
}

void emitMethod(ES& es, Decl& d, Member& m) {
    flushComments(es, m.line);
    std::string vis = visOf(m.mods);
    bool isIface = es.isInterfaceCtx;
    bool ovr = false, virt = false;
    std::string name = methodRename(es, d, m, &ovr, &virt);
    std::string modsPrefix = isIface ? "" : vis + " ";
    if (hasMod(m.mods, "static")) modsPrefix += "static ";
    else if (hasMod(m.mods, "abstract")) modsPrefix += "abstract ";
    else if (ovr) modsPrefix += "override ";
    else if (virt) modsPrefix += "virtual ";
    if (hasMod(m.mods, "synchronized")) {
        es.ctx.needCompilerServices = true;
        es.line("[MethodImpl(MethodImplOptions.Synchronized)]");
    }
    bool prevIface = es.isInterfaceCtx;
    emitMethodCommon(es, d, m, name, modsPrefix, m.body != nullptr);
    es.isInterfaceCtx = prevIface;
}

void emitCtor(ES& es, Decl& d, Member& m) {
    flushComments(es, m.line);
    emitAnnotations(es, m.anns);
    std::string vis = visOf(m.mods);
    std::string sig = vis + " " + d.name + "(" + paramListStr(es, m) + ")";
    std::string initChain;
    if (!m.baseArgs.empty()) {
        initChain = " : base(";
        for (size_t i = 0; i < m.baseArgs.size(); i++) {
            if (i) initChain += ", ";
            initChain += captureExprText(es, *m.baseArgs[i]);
        }
        initChain += ")";
    }
    es.wi();
    es.w(sig);
    if (m.body && m.body->k == SK::Block && !m.body->block.empty()) {
        Stmt& first = *m.body->block.front();
        if (first.k == SK::ExprStmt && first.e && first.e->k == EK::CtorCall) {
            Expr& cc = *first.e;
            std::vector<std::string> parts;
            for (size_t i = 0; i < cc.args.size(); i++) {
                parts.push_back(captureExprText(es, *cc.args[i]));
            }
            initChain = std::string(" : ") + (cc.name == "super" ? "base(" : "this(") + joinV(parts, ", ") + ")";
            m.body->block.erase(m.body->block.begin());
        }
    }
    bool hasInitBlocks = false;
    for (auto& mm : d.members)
        if (mm.mk == 3 && !mm.isStaticInit) hasInitBlocks = true;
    es.w(initChain);
    flushTrailing(es, m.line);
    es.nl();
    es.line("{");
    es.indent++;
    if (hasInitBlocks) es.line("__InstanceInit();");
    std::vector<VarInfo> pushed;
    for (auto& p : m.params) {
        VarInfo vi;
        vi.name = p.second;
        vi.ty = toInfo(es.ctx, p.first);
        es.scope.push_back(vi);
        pushed.push_back(vi);
    }
    if (m.body) emitBodyBlock(es, *m.body);
    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
    es.indent--;
    es.line("}");
}

void emitBodyBlock(ES& es, Stmt& body) {
    if (body.k == SK::Block) {
        for (auto& s : body.block) emitStmt(es, *s);
    } else {
        emitStmt(es, body);
    }
}

void flushComments(ES& es, int line) {
    while (es.ctx.nextComment < es.ctx.comments.size()) {
        Comment& c = es.ctx.comments[es.ctx.nextComment];
        if (c.line < line) {
            if (c.block) {
                std::vector<std::string> lines = splitStr(c.text, '\n');
                for (auto& ln : lines) {
                    es.wi();
                    if (startsWith(trimS(ln), "*")) es.w(" " + trimS(ln));
                    else es.w(ln);
                    es.nl();
                }
            } else {
                es.wi();
                es.w(c.text);
                es.nl();
            }
            es.ctx.nextComment++;
        } else break;
    }
}

void flushTrailing(ES& es, int line) {
    while (es.ctx.nextComment < es.ctx.comments.size()) {
        Comment& c = es.ctx.comments[es.ctx.nextComment];
        if (c.line == line) {
            if (c.block) es.w(" /*" + c.text + "*/");
            else es.w(" " + c.text);
            es.ctx.nextComment++;
        } else break;
    }
}

void emitNestedType(ES& es, Member& m) {
    if (m.nested) emitType(es, *m.nested);
}

void emitWhere(ES& es, const std::vector<TP>& tps) {
    for (auto& tp : tps) {
        if (tp.hasBound && !tp.bound.empty()) {
            es.w(" where " + tp.name + " : " + csTypeStr(es.ctx, tp.bound));
        }
    }
}

void emitTypeHeader(ES& es, Decl& d, const std::string& kind) {
    std::string vis = visOf(d.mods);
    std::string mods = vis;
    if (hasMod(d.mods, "abstract")) mods += " abstract";
    if (hasMod(d.mods, "final")) mods += " sealed";
    es.wi();
    es.w(mods + " " + kind + " " + d.name);
    if (!d.tparams.empty()) {
        es.w("<");
        for (size_t i = 0; i < d.tparams.size(); i++) {
            if (i) es.w(", ");
            es.w(d.tparams[i].name);
        }
        es.w(">");
    }
    for (auto& tp : d.tparams) es.typeParams.insert(tp.name);
    std::string inherits;
    TypeRef effExt = d.extends;
    bool extDropped = false;
    if (!effExt.empty()) {
        std::string bs = firstSeg(effExt.name);
        if (bs == "Object") extDropped = true;
        else if (es.ctx.samDelegates.count(bs)) extDropped = true;
        if (!extDropped) inherits += csTypeStr(es.ctx, effExt);
    }
    for (auto& imp : d.impls) {
        std::string iname = firstSeg(imp.name);
        if (es.ctx.samDelegates.count(iname)) continue;
        if (iname == "Serializable" || iname == "Externalizable") continue;
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    emitWhere(es, d.tparams);
}

void emitTypeBody(ES& es, Decl& d) {
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    flushComments(es, d.line);
    bool hasInstInit = false;
    for (auto& m : d.members)
        if (m.mk == 3 && !m.isStaticInit) hasInstInit = true;
    bool instInitEmitted = false;
    for (auto& m : d.members) {
        if (m.mk == 0) emitField(es, m);
        else if (m.mk == 1) emitMethod(es, d, m);
        else if (m.mk == 2) emitCtor(es, d, m);
        else if (m.mk == 3) {
            if (m.isStaticInit || !instInitEmitted) {
                emitInitBlock(es, m, d);
                if (!m.isStaticInit) instInitEmitted = true;
            }
        } else if (m.mk == 4) emitNestedType(es, m);
    }
    if (hasInstInit) {
    }
    es.indent--;
    es.line("}");
}

void emitRecord(ES& es, Decl& d) {
    Member* compact = nullptr;
    Member* recCtor = nullptr;
    for (auto& m : d.members) {
        if (m.mk == 2 && hasMod(m.mods, "__recordctor") && !m.compactCtor) recCtor = &m;
        if (m.mk == 2 && m.compactCtor) compact = &m;
    }
    std::vector<std::pair<TypeRef, std::string>> comps;
    if (recCtor) comps = recCtor->params;
    std::string savedClsR = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " sealed class " + d.name);
    if (!d.tparams.empty()) {
        es.w("<");
        for (size_t i = 0; i < d.tparams.size(); i++) {
            if (i) es.w(", ");
            es.w(d.tparams[i].name);
        }
        es.w(">");
    }
    for (auto& tp : d.tparams) es.typeParams.insert(tp.name);
    std::string inherits;
    for (auto& imp : d.impls) {
        if (firstSeg(imp.name) == "Serializable") continue;
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    for (auto& comp : comps) {
        es.wi();
        es.w("public " + csTypeStr(es.ctx, comp.first) + " " + pascalCase(comp.second) + " { get; }");
        es.nl();
    }
    flushComments(es, d.line);
    es.wi();
    es.w("public " + d.name + "(" + paramListStr(es, *recCtor) + ")");
    es.nl();
    es.line("{");
    es.indent++;
    if (compact && compact->body) emitBodyBlock(es, *compact->body);
    for (auto& comp : comps) {
        es.wi();
        es.w(pascalCase(comp.second) + " = " + csIdent(comp.second) + ";");
        es.nl();
    }
    es.indent--;
    es.line("}");
    es.wi();
    std::string parts;
    for (auto& comp : comps) {
        if (!parts.empty()) parts += ", ";
        parts += comp.second + "={" + pascalCase(comp.second) + "}";
    }
    es.w("public override string ToString() => $\"" + d.name + "[" + parts + "]\";");
    es.nl();
    es.wi();
    std::string eq = "other is " + d.name + " p";
    for (auto& comp : comps) {
        eq += " && " + pascalCase(comp.second) + " == p." + pascalCase(comp.second);
    }
    es.w("public override bool Equals(object other) => " + eq + ";");
    es.nl();
    es.wi();
    std::string hc = "System.HashCode.Combine(";
    bool firstC = true;
    for (auto& comp : comps) {
        if (!firstC) hc += ", ";
        firstC = false;
        hc += pascalCase(comp.second);
    }
    hc += ")";
    if (comps.empty()) hc = "base.GetHashCode()";
    es.w("public override int GetHashCode() => " + hc + ";");
    es.nl();
    if (!comps.empty()) {
        es.wi();
        std::string dc;
        for (auto& comp : comps) {
            if (!dc.empty()) dc += ", ";
            dc += "out " + csTypeStr(es.ctx, comp.first) + " " + csIdent(comp.second);
        }
        es.w("public void Deconstruct(" + dc + ")");
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& comp : comps) {
            es.wi();
            es.w(csIdent(comp.second) + " = " + pascalCase(comp.second) + ";");
            es.nl();
        }
        es.indent--;
        es.line("}");
    }
    for (auto& m : d.members) {
        if (m.mk == 2 && (hasMod(m.mods, "__recordctor") || m.compactCtor)) continue;
        if (m.mk == 0) emitField(es, m);
        else if (m.mk == 1) emitMethod(es, d, m);
        else if (m.mk == 2) emitCtor(es, d, m);
        else if (m.mk == 4) emitNestedType(es, m);
    }
    es.indent--;
    es.line("}");
    es.cls = savedClsR;
}

void emitEnumSimple(ES& es, Decl& d) {
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " enum " + d.name);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    for (size_t i = 0; i < d.consts.size(); i++) {
        flushComments(es, d.consts[i].line);
        es.wi();
        es.w(csIdent(d.consts[i].name));
        if (i + 1 < d.consts.size()) es.w(",");
        flushTrailing(es, d.consts[i].line);
        es.nl();
    }
    es.indent--;
    es.line("}");
}

void emitEnumClass(ES& es, Decl& d) {
    std::string savedCls = es.cls;
    es.cls = d.name;
    std::string savedClsR = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " class " + d.name);
    std::string inherits;
    for (auto& imp : d.impls) {
        if (firstSeg(imp.name) == "Serializable") continue;
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    es.line("private static int __ord = 0;");
    es.line("public readonly int Ordinal;");
    es.line("public readonly string Name;");
    std::vector<Member> ctors;
    bool hasCtor = false;
    for (auto& m : d.members)
        if (m.mk == 2) hasCtor = true;
    for (auto& m : d.members) {
        if (m.mk == 0) emitField(es, m);
    }
    if (!hasCtor) {
        es.wi();
        es.w("public " + d.name + "(string __name)");
        es.nl();
        es.line("{");
        es.indent++;
        es.line("Ordinal = __ord++;");
        es.line("Name = __name;");
        es.indent--;
        es.line("}");
    } else {
        for (auto& m : d.members) {
            if (m.mk != 2) continue;
            flushComments(es, m.line);
            es.wi();
            es.w("public " + d.name + "(string __name" +
                 (m.params.empty() ? "" : ", " + paramListStr(es, m)) + ")");
            es.nl();
            es.line("{");
            es.indent++;
            es.line("Ordinal = __ord++;");
            es.line("Name = __name;");
            std::vector<VarInfo> pushed;
            for (auto& p : m.params) {
                VarInfo vi;
                vi.name = p.second;
                vi.ty = toInfo(es.ctx, p.first);
                es.scope.push_back(vi);
                pushed.push_back(vi);
            }
            if (m.body) emitBodyBlock(es, *m.body);
            for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
            es.indent--;
            es.line("}");
        }
    }
    for (auto& c : d.consts) {
        flushComments(es, c.line);
        es.wi();
        std::string ctorCall;
        if (hasCtor) {
            ctorCall = "new " + d.name + "(\"" + c.name + "\"";
            for (auto& a : c.args) {
                ctorCall += ", ";
                ctorCall += captureExprText(es, *a);
            }
            ctorCall += ")";
        } else {
            ctorCall = "new " + d.name + "(\"" + c.name + "\")";
        }
        if (c.body) {
            es.w("public static readonly " + d.name + " " + csIdent(c.name) + " = new __E_" + c.name + "(\"" + c.name + "\"");
            for (auto& a : c.args) {
                es.w(", ");
                emitExpr(es, *a);
            }
            es.w(");");
            flushTrailing(es, c.line);
            es.nl();
        } else {
            es.w("public static readonly " + d.name + " " + csIdent(c.name) + " = " + ctorCall + ";");
            flushTrailing(es, c.line);
            es.nl();
        }
    }
    for (auto& c : d.consts) {
        if (c.body) {
            flushComments(es, c.line);
            es.wi();
            es.w("private sealed class __E_" + c.name + " : " + d.name);
            es.nl();
            es.line("{");
            es.indent++;
            es.wi();
            const Member* matchCtor = nullptr;
            for (auto& m : d.members)
                if (m.mk == 2 && m.params.size() == c.args.size()) {
                    matchCtor = &m;
                    break;
                }
            std::string ps = "string __n";
            std::string baseArgs;
            if (matchCtor) {
                for (auto& p : matchCtor->params) {
                    ps += ", " + csTypeStr(es.ctx, p.first) + " " + csIdent(p.second);
                    baseArgs += ", " + csIdent(p.second);
                }
            }
            es.w("public __E_" + c.name + "(" + ps + ") : base(__n" + baseArgs + ")");
            es.nl();
            es.line("{");
            es.indent++;
            if (c.body) {
                for (auto& m : c.body->members) {
                    if (m.mk == 3 && m.body) emitBodyBlock(es, *m.body);
                }
            }
            es.indent--;
            es.line("}");
            for (auto& m : c.body->members) {
                if (m.mk == 1) {
                    bool ovr = false, virt = false;
                    std::string name = methodRename(es, d, m, &ovr, &virt);
                    if (!ovr)
                        for (auto& em : d.members)
                            if (em.mk == 1 && em.name == m.name && em.params.size() == m.params.size() &&
                                !hasMod(em.mods, "static"))
                                ovr = true;
                    std::string modsPrefix = "public ";
                    if (ovr) modsPrefix += "override ";
                    else modsPrefix += "virtual ";
                    bool hasWhere = false;
                    std::string tp = tparamsStr(es, m.tparams, &hasWhere);
                    std::set<std::string> savedTypeParams = es.typeParams;
                    for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
                    es.wi();
                    es.w(modsPrefix + csTypeStr(es.ctx, m.ty) + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")");
                    es.nl();
                    es.line("{");
                    es.indent++;
                    std::vector<VarInfo> pushed;
                    for (auto& p : m.params) {
                        VarInfo vi;
                        vi.name = p.second;
                        vi.ty = toInfo(es.ctx, p.first);
                        es.scope.push_back(vi);
                        pushed.push_back(vi);
                    }
                    if (m.body) emitBodyBlock(es, *m.body);
                    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
                    es.typeParams = savedTypeParams;
                    es.indent--;
                    es.line("}");
                }
            }
            es.indent--;
            es.line("}");
        }
    }
    bool instInit = false;
    for (auto& m : d.members)
        if (m.mk == 3 && !m.isStaticInit) instInit = true;
    for (auto& m : d.members) {
        if (m.mk == 3) {
            emitInitBlock(es, m, d);
        }
        if (m.mk == 1) {
            bool ovr = false, virt = false;
            std::string name = methodRename(es, d, m, &ovr, &virt);
            bool byConst = false;
            if (!hasMod(m.mods, "static"))
                for (auto& c : d.consts)
                    if (c.body)
                        for (auto& bm : c.body->members)
                            if (bm.mk == 1 && bm.name == m.name && bm.params.size() == m.params.size())
                                byConst = true;
            std::string modsPrefix = "public ";
            if (hasMod(m.mods, "static")) modsPrefix += "static ";
            else if (hasMod(m.mods, "abstract")) modsPrefix += "abstract ";
            else if (ovr) modsPrefix += "override ";
            else if (virt || byConst) modsPrefix += "virtual ";
            bool hasWhere = false;
            std::string tp = tparamsStr(es, m.tparams, &hasWhere);
            std::set<std::string> savedTypeParams = es.typeParams;
            for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
            flushComments(es, m.line);
            es.wi();
            es.w(modsPrefix + csTypeStr(es.ctx, m.ty) + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")");
            flushTrailing(es, m.line);
            es.nl();
            if (!m.body) {
                es.wi();
                es.w(";");
                es.nl();
            } else {
                es.line("{");
                es.indent++;
                std::vector<VarInfo> pushed;
                for (auto& p : m.params) {
                    VarInfo vi;
                    vi.name = p.second;
                    vi.ty = toInfo(es.ctx, p.first);
                    es.scope.push_back(vi);
                    pushed.push_back(vi);
                }
                emitBodyBlock(es, *m.body);
                for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
                es.typeParams = savedTypeParams;
                es.indent--;
                es.line("}");
            }
        }
        if (m.mk == 4) emitNestedType(es, m);
    }
    bool userToString = false;
    bool userCompareTo = false;
    for (auto& m : d.members) {
        if (m.mk == 1 && m.name == "toString" && m.params.empty()) userToString = true;
        if (m.mk == 1 && m.name == "compareTo" && m.params.size() == 1) userCompareTo = true;
    }
    if (!userToString) es.line("public override string ToString() => Name;");
    if (!userCompareTo) es.line("public int CompareTo(" + d.name + " other) => Ordinal - other.Ordinal;");
    std::string vals;
    for (size_t i = 0; i < d.consts.size(); i++) {
        if (i) vals += ", ";
        vals += csIdent(d.consts[i].name);
    }
    es.line("public static " + d.name + "[] Values() => new " + d.name + "[] { " + vals + " };");
    es.line("public static " + d.name + " ValueOf(string name) => Array.Find(Values(), c => c.Name == name);");
    es.indent--;
    es.line("}");
    es.cls = savedCls;
    (void)instInit;
}

void emitAnnotationDecl(ES& es, Decl& d) {
    flushComments(es, d.line);
    es.wi();
    es.w(visOf(d.mods) + " class " + d.name + " : Attribute");
    es.nl();
    es.line("{");
    es.indent++;
    for (auto& m : d.members) {
        if (m.mk == 1) {
            es.wi();
            std::string def;
            if (m.body && m.body->k == SK::Block && m.body->block.size() == 1 &&
                m.body->block[0]->k == SK::Return && m.body->block[0]->e) {
                def = " = " + captureExprText(es, *m.body->block[0]->e);
            }
            es.w("public " + csTypeStr(es.ctx, m.ty) + " " + pascalCase(m.name) + " { get; set; }" + (def.empty() ? "" : def + ";"));
            es.nl();
        }
    }
    es.indent--;
    es.line("}");
}

void emitInterface(ES& es, Decl& d) {
    std::string savedCls = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    es.wi();
    es.w(visOf(d.mods) + " interface " + d.name);
    if (!d.tparams.empty()) {
        es.w("<");
        for (size_t i = 0; i < d.tparams.size(); i++) {
            if (i) es.w(", ");
            es.w(d.tparams[i].name);
        }
        es.w(">");
    }
    for (auto& tp : d.tparams) es.typeParams.insert(tp.name);
    std::string inherits;
    if (!d.extends.empty()) {
        inherits += csTypeStr(es.ctx, d.extends);
    }
    for (auto& imp : d.impls) {
        if (!inherits.empty()) inherits += ", ";
        inherits += csTypeStr(es.ctx, imp, true);
    }
    if (!inherits.empty()) es.w(" : " + inherits);
    emitWhere(es, d.tparams);
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    bool prev = es.isInterfaceCtx;
    es.isInterfaceCtx = true;
    for (auto& m : d.members) {
        if (m.mk == 0) {
            bool constAble = isConstAble(es, m);
            for (size_t i = 0; i < m.names.size(); i++) {
                flushComments(es, m.line);
                es.wi();
                std::string t = csTypeStr(es.ctx, m.ty);
                for (int k = 0; k < m.fdims[i]; k++) t += "[]";
                std::string mods = constAble ? "const " : "static readonly ";
                es.w(mods + t + " " + csIdent(m.names[i]));
                if (m.finits[i]) {
                    es.w(" = ");
                    TypeInfo expect;
                    expect.known = true;
                    expect.name = csTypeStr(es.ctx, m.ty);
                    emitExpr(es, *m.finits[i], 0, expect);
                }
                es.w(";");
                es.nl();
            }
        } else if (m.mk == 1) {
            bool ovr = false, virt = false;
            std::string name = methodRename(es, d, m, &ovr, &virt);
            std::string modsPrefix;
            if (hasMod(m.mods, "static")) modsPrefix = "static ";
            else if (hasMod(m.mods, "private")) modsPrefix = "private ";
            bool hasWhere = false;
            std::string tp = tparamsStr(es, m.tparams, &hasWhere);
            std::set<std::string> savedTypeParams = es.typeParams;
            for (auto& tp2 : m.tparams) es.typeParams.insert(tp2.name);
            flushComments(es, m.line);
            es.wi();
            es.w(modsPrefix + csTypeStr(es.ctx, m.ty) + " " + csIdent(name) + tp + "(" + paramListStr(es, m) + ")");
            es.nl();
            if (m.body) {
                es.line("{");
                es.indent++;
                std::vector<VarInfo> pushed;
                for (auto& p : m.params) {
                    VarInfo vi;
                    vi.name = p.second;
                    vi.ty = toInfo(es.ctx, p.first);
                    es.scope.push_back(vi);
                    pushed.push_back(vi);
                }
                emitBodyBlock(es, *m.body);
                for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
                es.indent--;
                es.line("}");
            } else {
                es.wi();
                es.w(";");
                es.nl();
            }
            es.typeParams = savedTypeParams;
        } else if (m.mk == 4) {
            emitNestedType(es, m);
        }
    }
    es.isInterfaceCtx = prev;
    es.indent--;
    es.line("}");
}

void emitClass(ES& es, Decl& d) {
    std::string savedCls = es.cls;
    es.cls = d.name;
    flushComments(es, d.line);
    emitAnnotations(es, d.anns);
    emitTypeHeader(es, d, "class");
    es.w("");
    es.nl();
    es.line("{");
    es.indent++;
    bool prev = es.isInterfaceCtx;
    es.isInterfaceCtx = false;
    bool instInitEmitted = false;
    for (auto& m : d.members) {
        if (m.mk == 0) emitField(es, m);
        else if (m.mk == 1) emitMethod(es, d, m);
        else if (m.mk == 2) emitCtor(es, d, m);
        else if (m.mk == 3) {
            if (m.isStaticInit || !instInitEmitted) {
                emitInitBlock(es, m, d);
                if (!m.isStaticInit) instInitEmitted = true;
            }
        } else if (m.mk == 4) emitNestedType(es, m);
    }
    es.isInterfaceCtx = prev;
    es.indent--;
    es.line("}");
    es.cls = savedCls;
}

void emitType(ES& es, Decl& d) {
    if (d.dk == 1 && es.ctx.samDelegates.count(d.name)) {
        std::string sig = delegateSigFor(es.ctx, d);
        if (!sig.empty()) {
            flushComments(es, d.line);
            emitAnnotations(es, d.anns);
            es.wi();
            es.w(visOf(d.mods) + " delegate " + sig + ";");
            es.nl();
            return;
        }
    }
    if (d.dk == 0) emitClass(es, d);
    else if (d.dk == 1) emitInterface(es, d);
    else if (d.dk == 2) {
        if (es.ctx.simpleEnums.count(d.name)) emitEnumSimple(es, d);
        else emitEnumClass(es, d);
    } else if (d.dk == 3) emitRecord(es, d);
    else if (d.dk == 4) emitAnnotationDecl(es, d);
}

std::string delegateSigFor(Ctx& ctx, Decl& iface) {
    for (auto& m : iface.members) {
        if (m.mk == 1 && !m.body) {
            std::string r;
            r = csTypeStr(ctx, m.ty) + " " + iface.name + "(";
            for (size_t i = 0; i < m.params.size(); i++) {
                if (i) r += ", ";
                if (m.varargs && i == m.params.size() - 1) {
                    TypeRef at = m.params[i].first;
                    if (at.dims == 0) at.dims = 1;
                    r += "params " + csTypeStr(ctx, at) + " " + m.params[i].second;
                } else {
                    r += csTypeStr(ctx, m.params[i].first) + " " + m.params[i].second;
                }
            }
            r += ")";
            return r;
        }
    }
    return "";
}

void flushRemainingComments(Ctx& ctx, std::ostringstream& out) {
    while (ctx.nextComment < ctx.comments.size()) {
        Comment& c = ctx.comments[ctx.nextComment++];
        if (c.block) out << "/*" << c.text << "*/\n";
        else out << c.text << "\n";
    }
}

std::string convertUnit(Ctx& ctx, Unit& u) {
    ES es(ctx);
    for (auto& d : u.types) emitType(es, *d);
    std::string helpers;
    if (ctx.needCollectionsSpec) {
        ctx.needLinq = true;
        helpers += R"hlp(static class __JHelp
{
    public static T Poll<T>(LinkedList<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q.First.Value;
        q.RemoveFirst();
        return v;
    }

    public static T Poll<T>(List<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q[0];
        q.RemoveAt(0);
        return v;
    }

    public static T PollLast<T>(LinkedList<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q.Last.Value;
        q.RemoveLast();
        return v;
    }

    public static T PollLast<T>(List<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q[q.Count - 1];
        q.RemoveAt(q.Count - 1);
        return v;
    }

    public static T PollPQ<T>(List<T> q)
    {
        if (q.Count == 0) return default(T);
        T v = q.Min();
        q.Remove(v);
        return v;
    }

    public static T[] CopyOf<T>(T[] a, int n)
    {
        T[] d = new T[Math.Max(0, n)];
        Array.Copy(a, d, Math.Min(a.Length, Math.Max(0, n)));
        return d;
    }

    public static T[] CopyOfRange<T>(T[] a, int from, int to)
    {
        T[] d = new T[Math.Max(0, to - from)];
        for (int i = from; i < to && i < a.Length; i++) d[i - from] = a[i];
        return d;
    }

    public static IEnumerable<int> IntRange(int start, int end, bool inclusive)
    {
        for (long value = start; inclusive ? value <= end : value < end; value++)
            yield return (int)value;
    }

    public static IEnumerable<long> LongRange(long start, long end, bool inclusive)
    {
        for (long value = start; inclusive ? value <= end : value < end;)
        {
            yield return value;
            if (value == long.MaxValue) yield break;
            value++;
        }
    }

    public static List<T> AsList<T>(params T[] a)
    {
        return new List<T>(a);
    }

    public static string ArrayToString<T>(T[] a)
    {
        return "[" + string.Join(", ", a) + "]";
    }

    public static bool ArrayEquals<T>(T[] a, T[] b)
    {
        return a.SequenceEqual(b);
    }

    public static void Sort<T>(List<T> l)
    {
        l.Sort();
    }

    public static void Sort<T>(List<T> l, Comparison<T> c)
    {
        l.Sort(c);
    }

    public static void Reverse<T>(List<T> l)
    {
        l.Reverse();
    }

    public static void Swap<T>(IList<T> l, int i, int j)
    {
        (l[i], l[j]) = (l[j], l[i]);
    }
}

)hlp";
    }
    if (ctx.needScanner) {
        helpers += R"hlp(class __Scanner
{
    readonly System.IO.TextReader r;
    string[] toks = new string[0];
    int pos = 0;
    bool midLine = false;

    public __Scanner(System.IO.TextReader src) { r = src; }

    bool refill()
    {
        while (pos >= toks.Length)
        {
            string line = r.ReadLine();
            if (line == null) { toks = new string[0]; pos = 0; return false; }
            toks = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
            pos = 0;
            midLine = false;
        }
        return true;
    }

    public string next() { if (!refill()) return null; midLine = true; return toks[pos++]; }
    public bool hasNext() { return refill(); }
    public bool hasNextLine() { return pos < toks.Length || r.Peek() != -1; }
    public int nextInt() { return int.Parse(next()); }
    public long nextLong() { return long.Parse(next()); }
    public double nextDouble() { return double.Parse(next(), System.Globalization.CultureInfo.InvariantCulture); }
    public bool nextBoolean() { return bool.Parse(next()); }

    public string nextLine()
    {
        if (midLine)
        {
            if (pos >= toks.Length) { midLine = false; return ""; }
            string rest = string.Join(" ", toks, pos, toks.Length - pos);
            pos = toks.Length;
            midLine = false;
            return rest;
        }
        string line = r.ReadLine();
        return line == null ? "" : line;
    }

    public void close() { }
}

)hlp";
    }
    std::string body = helpers + es.out.str();
    while (!body.empty() && body.back() == '\n') body.pop_back();
    body += "\n";
    std::set<std::string> usings;
    usings.insert("System");
    usings.insert("System.Collections.Generic");
    if (ctx.needLinq) usings.insert("System.Linq");
    if (ctx.needText) usings.insert("System.Text");
    if (ctx.needIO) usings.insert("System.IO");
    if (ctx.needRegex) usings.insert("System.Text.RegularExpressions");
    if (ctx.needDiag) usings.insert("System.Diagnostics");
    if (ctx.needCompilerServices) usings.insert("System.Runtime.CompilerServices");
    if (ctx.needGlobalization) usings.insert("System.Globalization");
    if (ctx.needNumerics) usings.insert("System.Numerics");
    if (ctx.needThreading) usings.insert("System.Threading");
    if (ctx.needReflection) usings.insert("System.Reflection");
    for (auto& ns : ctx.usings) {
        if (ns == "System" || ns == "System.Collections.Generic") continue;
        usings.insert(ns);
    }
    std::ostringstream finalOut;
    for (auto& us : usings) finalOut << "using " << us << ";\n";
    finalOut << "\n";
    if (!u.package.empty()) {
        finalOut << "namespace " << u.package << "\n{\n";
        std::istringstream in(body);
        std::string ln;
        while (std::getline(in, ln)) {
            if (ln.empty()) finalOut << "\n";
            else finalOut << "    " << ln << "\n";
        }
        finalOut << "}\n";
    } else {
        finalOut << body;
    }
    flushRemainingComments(ctx, finalOut);
    return finalOut.str();
}
bool stmtTerminates(Stmt& st);
bool stmtTerminatesOrBreak(CaseArm& arm);
void emitArgs(ES& es, std::vector<std::unique_ptr<Expr>>& args);
bool emitExprStmt(ES& es, Expr& e);
void emitLocalVar(ES& es, Stmt& st);
void emitFor(ES& es, Stmt& st);
void emitForeach(ES& es, Stmt& st);
bool switchENeedsStmtForm(Expr& e);
void emitSwitchEAsStmt(ES& es, Expr& sw, const std::string& target);

bool stmtTerminates(Stmt& st) {
    switch (st.k) {
        case SK::Return:
        case SK::Throw:
        case SK::Break:
        case SK::Continue:
            return true;
        case SK::Block:
            if (st.block.empty()) return false;
            return stmtTerminates(*st.block.back());
        case SK::If:
            return st.body && st.body2 && stmtTerminates(*st.body) && stmtTerminates(*st.body2);
        case SK::Label:
            return st.body && stmtTerminates(*st.body);
        case SK::Switch: {
            if (st.arms.empty()) return false;
            bool hasDefault = false;
            for (auto& arm : st.arms) {
                if (arm.arrow) {
                    if (arm.isBlockVal) {
                        if (arm.body.empty() || !stmtTerminates(*arm.body.back())) return false;
                    } else return false;
                } else {
                    if (arm.body.empty() || !stmtTerminates(*arm.body.back())) return false;
                }
                if (arm.isDefault) hasDefault = true;
            }
            return hasDefault;
        }
        case SK::Try:
            if (st.fin) return stmtTerminates(*st.fin);
            return false;
        case SK::Sync:
            return st.body && stmtTerminates(*st.body);
        default:
            return false;
    }
}

void emitBodyStmt(ES& es, Stmt& st) {
    if (st.k == SK::Block) {
        es.line("{");
        es.indent++;
        for (auto& s : st.block) emitStmt(es, *s);
        es.indent--;
        es.line("}");
    } else {
        es.indent++;
        emitStmt(es, st);
        es.indent--;
    }
}

std::string captureExpr(ES& es, Expr& e, int prec = 0, const TypeInfo& expect = TypeInfo()) {
    return captureExprText(es, e, prec, expect);
}

std::string enumLabelText(ES& es, Expr& l, const std::string& subjType) {
    std::string lt = captureExpr(es, l, 20);
    if (!subjType.empty() && l.k == EK::Ident) {
        auto it = es.ctx.enumConstOwner.find(l.name);
        if (it != es.ctx.enumConstOwner.end() && it->second == subjType) return subjType + "." + lt;
    }
    return lt;
}

void emitEnumClassSwitch(ES& es, Stmt& st, const std::string& enumName) {
    std::string subj = captureExpr(es, *st.subject, 20);
    es.wi();
    es.w("{");
    es.nl();
    es.indent++;
    std::string swv = "__sw" + std::to_string(es.tmpCounter);
    es.tmpCounter++;
    es.wi();
    es.w("var " + swv + " = " + subj + ";");
    es.nl();
    struct SwSection {
        std::vector<Expr*> labels;
        const std::vector<TypeRef>* patLabels = nullptr;
        const std::string* patName = nullptr;
        bool nullLabel = false;
        Expr* guard = nullptr;
        bool isDefault = false;
        std::vector<std::unique_ptr<Stmt>>* body = nullptr;
        bool arrow = false;
        Expr* value = nullptr;
    };
    std::vector<SwSection> sections;
    std::vector<Expr*> pending;
    for (auto& arm : st.arms) {
        bool labelOnly = !arm.arrow && arm.body.empty() && !arm.isDefault && !arm.nullLabel &&
                         arm.patLabels.empty() && !arm.labels.empty() && !arm.guard && !arm.value;
        if (labelOnly) {
            for (auto& l : arm.labels) pending.push_back(l.get());
            continue;
        }
        SwSection sec;
        for (auto& l : pending) sec.labels.push_back(l);
        pending.clear();
        for (auto& l : arm.labels) sec.labels.push_back(l.get());
        sec.patLabels = &arm.patLabels;
        sec.patName = &arm.patName;
        sec.nullLabel = arm.nullLabel;
        sec.guard = arm.guard.get();
        sec.isDefault = arm.isDefault;
        sec.body = &arm.body;
        sec.arrow = arm.arrow;
        sec.value = arm.value.get();
        sections.push_back(std::move(sec));
    }
    SwSection* defaultArm = nullptr;
    bool firstSec = true;
    for (auto& sec : sections) {
        if (sec.isDefault && sec.labels.empty() && sec.patLabels->empty() && !sec.nullLabel) {
            defaultArm = &sec;
            continue;
        }
        std::vector<std::string> conds;
        for (auto* l : sec.labels) conds.push_back(swv + " == " + enumLabelText(es, *l, enumName));
        if (sec.nullLabel) conds.push_back(swv + " == null");
        for (auto& pl : *sec.patLabels)
            conds.push_back(swv + " is " + csTypeStr(es.ctx, pl, true) +
                            (sec.patName->empty() ? " _" : " " + csIdent(*sec.patName)));
        es.wi();
        es.w(firstSec ? "if (" : "else if (");
        firstSec = false;
        bool firstL = true;
        for (auto& c : conds) {
            if (!firstL) es.w(" || ");
            firstL = false;
            es.w(c);
        }
        es.w(")");
        if (sec.guard) {
            std::string g = captureExpr(es, *sec.guard, 0);
            es.w(firstL ? "(" : " && (");
            es.w(g);
            es.w(")");
        }
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& s : *sec.body) {
            if (s->k == SK::Break && s->label.empty()) continue;
            emitStmt(es, *s);
        }
        if (sec.arrow && sec.value) {
            es.wi();
            if (!emitExprStmt(es, *sec.value)) es.w(";");
            es.nl();
        }
        es.indent--;
        es.line("}");
    }
    if (defaultArm) {
        es.wi();
        es.w("else");
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& s : *defaultArm->body) {
            if (s->k == SK::Break && s->label.empty()) continue;
            emitStmt(es, *s);
        }
        es.indent--;
        es.line("}");
    }
    es.indent--;
    es.line("}");
}

void emitClassicSwitch(ES& es, Stmt& st) {
    TypeInfo subjTy = inferExprType(es, *st.subject);
    if (subjTy.known && es.ctx.enumClassTypes.count(subjTy.name)) {
        emitEnumClassSwitch(es, st, subjTy.name);
        return;
    }
    if (subjTy.known && es.ctx.simpleEnums.count(subjTy.name)) es.switchSubjectType = subjTy.name;
    std::string subj = captureExpr(es, *st.subject, 20);
    es.wi();
    es.w("switch (" + subj + ")");
    es.nl();
    es.line("{");
    es.indent++;
    std::vector<std::string> pendingLabels;
    for (size_t i = 0; i < st.arms.size(); i++) {
        CaseArm& arm = st.arms[i];
        if (!arm.arrow && arm.body.empty() && !arm.isDefault && !arm.nullLabel &&
            arm.patLabels.empty() && !arm.labels.empty()) {
            for (auto& l : arm.labels) pendingLabels.push_back(enumLabelText(es, *l, es.switchSubjectType));
            continue;
        }
        es.wi();
        bool first = true;
        if (arm.isDefault) {
            es.w("default:");
            first = false;
        }
        if (arm.nullLabel) {
            if (!first) es.w(" ");
            es.w("case null:");
            first = false;
        }
        for (auto& pl : arm.patLabels) {
            if (!first) es.w(" ");
            first = false;
            es.w("case " + csTypeStr(es.ctx, pl, true) +
                 (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)) + ":");
        }
        for (auto& l : arm.labels) {
            if (!first) es.w(" ");
            first = false;
            es.w("case " + enumLabelText(es, *l, es.switchSubjectType) + ":");
        }
        for (auto& pl : pendingLabels) es.w(" case " + pl + ":");
        pendingLabels.clear();
        if (arm.guard) es.w(" when " + captureExpr(es, *arm.guard, 20));
        es.nl();
        es.indent++;
        bool bodyTerm = !arm.body.empty() && stmtTerminates(*arm.body.back());
        for (auto& s : arm.body) emitStmt(es, *s);
        if (arm.arrow && arm.value) {
            auto exprStmt = std::make_unique<Stmt>();
            exprStmt->k = SK::ExprStmt;
            exprStmt->e = std::move(arm.value);
            emitStmt(es, *exprStmt);
            bodyTerm = false;
        }
        if (!bodyTerm) {
            std::string gotoTarget;
            bool nextExists = false;
            for (size_t j = i + 1; j < st.arms.size(); j++) {
                CaseArm& nx = st.arms[j];
                if (nx.isDefault) {
                    nextExists = true;
                    gotoTarget = "default";
                    break;
                }
                if (!nx.labels.empty()) {
                    nextExists = true;
                    gotoTarget = captureExpr(es, *nx.labels.front(), 20);
                    break;
                }
            }
            if (nextExists) {
                if (gotoTarget == "default") es.line("goto default;");
                else es.line("goto case " + gotoTarget + ";");
            } else {
                es.line("break;");
            }
        }
        es.indent--;
    }
    es.indent--;
    es.line("}");
    es.switchSubjectType = "";
}

bool stmtTerminatesOrBreak(CaseArm& arm) {
    if (arm.body.empty()) return false;
    Stmt& last = *arm.body.back();
    if (last.k == SK::Break && last.label.empty()) return true;
    return stmtTerminates(last);
}

void emitSwitch(ES& es, Stmt& st) {
    flushComments(es, st.line);
    TypeInfo subjTy = inferExprType(es, *st.subject);
    if (subjTy.known && es.ctx.enumClassTypes.count(subjTy.name)) {
        emitClassicSwitch(es, st);
        es.switchSubjectType = "";
        flushTrailing(es, st.line);
        return;
    }
    if (subjTy.known && es.ctx.simpleEnums.count(subjTy.name)) es.switchSubjectType = subjTy.name;
    if (st.arrowSw) {
        std::string subj = captureExpr(es, *st.subject, 20);
        es.wi();
        es.w("switch (" + subj + ")");
        es.nl();
        es.line("{");
        es.indent++;
        for (auto& arm : st.arms) {
            if (!arm.labels.empty() || !arm.patLabels.empty() || arm.nullLabel || arm.isDefault) {
                es.wi();
                bool first = true;
                if (arm.guard) {
                    es.w("case ");
                    bool g1 = true;
                    if (arm.isDefault) {
                        es.w("_");
                        g1 = false;
                    }
                    if (arm.nullLabel) {
                        if (!g1) es.w(" or ");
                        es.w("null");
                        g1 = false;
                    }
                    for (auto& pl : arm.patLabels) {
                        if (!g1) es.w(" or ");
                        g1 = false;
                        es.w(csTypeStr(es.ctx, pl, true) +
                             (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)));
                    }
                    for (auto& l : arm.labels) {
                        if (!g1) es.w(" or ");
                        g1 = false;
                        es.w(enumLabelText(es, *l, es.switchSubjectType));
                    }
                    if (g1) es.w("_");
                    es.w(" when " + captureExpr(es, *arm.guard, 20));
                    es.w(":");
                } else {
                    if (arm.isDefault) {
                        es.w("default:");
                        first = false;
                    }
                    if (arm.nullLabel) {
                        if (!first) es.w(" ");
                        es.w("case null:");
                        first = false;
                    }
                    for (auto& pl : arm.patLabels) {
                        if (!first) es.w(" ");
                        es.w("case " + csTypeStr(es.ctx, pl, true) +
                             (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)) + ":");
                        first = false;
                    }
                    for (auto& l : arm.labels) {
                        if (!first) es.w(" ");
                        first = false;
                        es.w("case " + enumLabelText(es, *l, es.switchSubjectType) + ":");
                    }
                    if (first) es.w("default:");
                }
                es.nl();
            }
            es.indent++;
            bool term = false;
            if (arm.value) {
                auto exprStmt = std::make_unique<Stmt>();
                exprStmt->k = SK::ExprStmt;
                exprStmt->e = std::move(arm.value);
                emitStmt(es, *exprStmt);
            }
            for (auto& s : arm.body) {
                emitStmt(es, *s);
            }
            if (!arm.body.empty()) {
                Stmt& last = *arm.body.back();
                term = last.k == SK::Break || last.k == SK::Return || last.k == SK::Throw ||
                       last.k == SK::Continue;
            }
            if (!term) es.line("break;");
            es.indent--;
        }
        es.indent--;
        es.line("}");
        es.switchSubjectType = "";
        flushTrailing(es, st.line);
        return;
    }
    emitClassicSwitch(es, st);
    flushTrailing(es, st.line);
}

void emitTry(ES& es, Stmt& st) {
    flushComments(es, st.line);
    size_t depth = 0;
    for (size_t i = 0; i < st.resources.size(); i++) {
        es.wi();
        std::string rt = csTypeStr(es.ctx, st.resources[i].first);
        if (rt == "var") rt = "var";
        es.w("using (" + rt + " " + csIdent(st.resources[i].second) + " = " +
             captureExpr(es, *st.resInits[i], 0) + ")");
        es.nl();
        es.line("{");
        es.indent++;
        depth++;
        es.scope.push_back({st.resources[i].second, toInfo(es.ctx, st.resources[i].first)});
    }
    bool needTry = !st.catches.empty() || st.fin;
    if (needTry) {
        es.wi();
        es.w("try");
        es.nl();
        es.wi();
        es.w("{");
        es.nl();
        es.indent++;
        if (st.body) emitBodyBlock(es, *st.body);
        es.indent--;
        es.wi();
        es.w("}");
        es.nl();
        for (auto& c : st.catches) {
            es.wi();
            if (c.types.size() == 1) {
                es.w("catch (" + csTypeStr(es.ctx, c.types[0]) + " " + csIdent(c.var) + ")");
            } else {
                std::string cond;
                for (size_t i = 0; i < c.types.size(); i++) {
                    if (i) cond += " || ";
                    cond += csIdent(c.var) + " is " + csTypeStr(es.ctx, c.types[i]);
                }
                es.w("catch (Exception " + csIdent(c.var) + ") when (" + cond + ")");
            }
            es.nl();
            es.wi();
            es.w("{");
            es.nl();
            es.indent++;
            {
                TypeInfo ct;
                ct.known = true;
                ct.name = c.types.size() == 1 ? csTypeStr(es.ctx, c.types[0]) : "Exception";
                es.scope.push_back({c.var, ct});
            }
            if (c.body) emitBodyBlock(es, *c.body);
            es.scope.pop_back();
            es.indent--;
            es.wi();
            es.w("}");
            es.nl();
        }
        if (st.fin) {
            es.wi();
            es.w("finally");
            es.nl();
            es.wi();
            es.w("{");
            es.nl();
            es.indent++;
            emitBodyBlock(es, *st.fin);
            es.indent--;
            es.wi();
            es.w("}");
            es.nl();
        }
    } else {
        if (st.body) emitBodyStmt(es, *st.body);
    }
    for (size_t i = 0; i < depth; i++) {
        es.indent--;
        es.line("}");
        if (!es.scope.empty()) es.scope.pop_back();
    }
    flushTrailing(es, st.line);
}

void emitStmt(ES& es, Stmt& st) {
    switch (st.k) {
        case SK::Block:
            flushComments(es, st.line);
            es.line("{");
            es.indent++;
            for (auto& s : st.block) emitStmt(es, *s);
            es.indent--;
            es.line("}");
            break;
        case SK::Empty:
            flushComments(es, st.line);
            es.wi();
            es.w(";");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::LocalClass:
            flushComments(es, st.line);
            break;
        case SK::ExprStmt:
            flushComments(es, st.line);
            es.wi();
            if (st.e) {
                if (!emitExprStmt(es, *st.e)) es.w(";");
            }
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::LocalVar:
            emitLocalVar(es, st);
            break;
        case SK::If:
            flushComments(es, st.line);
            es.wi();
            es.w("if (" + captureExpr(es, *st.e, 0) + ")");
            flushTrailing(es, st.line);
            es.nl();
            emitBodyStmt(es, *st.body);
            if (st.body2) {
                es.wi();
                es.w("else");
                es.nl();
                emitBodyStmt(es, *st.body2);
            }
            break;
        case SK::While:
            flushComments(es, st.line);
            es.wi();
            es.w("while (" + captureExpr(es, *st.e, 0) + ")");
            flushTrailing(es, st.line);
            es.nl();
            emitBodyStmt(es, *st.body);
            break;
        case SK::DoWhile:
            flushComments(es, st.line);
            es.wi();
            es.w("do");
            es.nl();
            emitBodyStmt(es, *st.body);
            es.wi();
            es.w("while (" + captureExpr(es, *st.e, 0) + ");");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::For:
            emitFor(es, st);
            break;
        case SK::Foreach:
            emitForeach(es, st);
            break;
        case SK::Break:
            flushComments(es, st.line);
            es.wi();
            if (st.label.empty()) es.w("break;");
            else if (es.activeLabels.count(st.label)) es.w("goto " + csIdent(st.label) + "__end;");
            else es.w("break;");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Continue:
            flushComments(es, st.line);
            es.wi();
            if (st.label.empty()) es.w("continue;");
            else if (es.activeLabels.count(st.label)) es.w("goto " + csIdent(st.label) + "__cont;");
            else es.w("continue;");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Return:
            flushComments(es, st.line);
            es.wi();
            if (st.e && st.e->k == EK::SwitchE && switchENeedsStmtForm(*st.e) && es.hasCurRet &&
                es.curRet.name != "void" && !es.curRet.empty() && !es.curRet.isVar) {
                std::string tmp = "__swret" + std::to_string(es.tmpCounter++);
                es.w(csTypeStr(es.ctx, es.curRet) + " " + tmp + " = default;");
                es.nl();
                emitSwitchEAsStmt(es, *st.e, tmp);
                es.wi();
                es.w("return " + tmp + ";");
                flushTrailing(es, st.line);
                es.nl();
                break;
            }
            es.w("return");
            if (st.e) {
                TypeInfo expect;
                if (es.hasCurRet && !es.curRet.isVar && !es.curRet.empty() &&
                    es.curRet.name != "void") {
                    expect = toInfo(es.ctx, es.curRet);
                }
                if (st.e->k == EK::Lit && st.e->lt == TT::Null && expect.known &&
                    es.typeParams.count(expect.name) > 0) {
                    es.w(" default");
                } else {
                    es.w(" ");
                    emitExpr(es, *st.e, 0, expect);
                }
            }
            es.w(";");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Throw:
            flushComments(es, st.line);
            es.wi();
            es.w("throw ");
            emitExpr(es, *st.e, 0);
            es.w(";");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Try:
            emitTry(es, st);
            break;
        case SK::Switch:
            emitSwitch(es, st);
            break;
        case SK::Sync:
            flushComments(es, st.line);
            es.wi();
            es.w("lock (" + captureExpr(es, *st.e, 0) + ")");
            es.nl();
            emitBodyStmt(es, *st.body);
            break;
        case SK::Assert:
            flushComments(es, st.line);
            es.ctx.needDiag = true;
            es.wi();
            es.w("Debug.Assert(" + captureExpr(es, *st.e, 0));
            if (st.e2) es.w(", " + captureExpr(es, *st.e2, 0));
            es.w(");");
            flushTrailing(es, st.line);
            es.nl();
            break;
        case SK::Label: {
            flushComments(es, st.line);
            bool isLoop = st.body && (st.body->k == SK::While || st.body->k == SK::DoWhile ||
                                      st.body->k == SK::For || st.body->k == SK::Foreach);
            es.wi();
            es.w(csIdent(st.label) + ":;");
            flushTrailing(es, st.line);
            es.nl();
            if (st.body) {
                if (isLoop) {
                    es.activeLabels.insert(st.label);
                    Stmt* loop = st.body.get();
                    if (!loop->body || loop->body->k != SK::Block) {
                        auto wrapped = std::make_unique<Stmt>();
                        wrapped->k = SK::Block;
                        wrapped->line = loop->line;
                        wrapped->block.push_back(std::move(loop->body));
                        loop->body = std::move(wrapped);
                    }
                    auto contLabel = std::make_unique<Stmt>();
                    contLabel->k = SK::Label;
                    contLabel->label = st.label + "__cont";
                    contLabel->line = st.line;
                    loop->body->block.push_back(std::move(contLabel));
                    emitStmt(es, *loop);
                    es.activeLabels.erase(st.label);
                } else if (st.body->k == SK::Block) {
                    es.activeLabels.insert(st.label);
                    es.line("{");
                    es.indent++;
                    for (auto& s : st.body->block) emitStmt(es, *s);
                    es.indent--;
                    es.line("}");
                    es.activeLabels.erase(st.label);
                } else {
                    es.activeLabels.insert(st.label);
                    emitStmt(es, *st.body);
                    es.activeLabels.erase(st.label);
                }
                es.wi();
                es.w(csIdent(st.label) + "__end: ;");
                es.nl();
            }
            break;
        }
    }
}

bool switchENeedsStmtForm(Expr& e) {
    if (e.k != EK::SwitchE) return false;
    for (auto& arm : e.cases) {
        if (!arm.isBlockVal) continue;
        bool onlyYield = arm.body.size() == 1 && arm.body[0]->k == SK::Return && arm.body[0]->isYield;
        if (!onlyYield) return true;
    }
    return false;
}

void emitSwitchEAsStmt(ES& es, Expr& sw, const std::string& target) {
    auto stmt = std::make_unique<Stmt>();
    stmt->k = SK::Switch;
    stmt->line = sw.line;
    stmt->subject = std::move(sw.a);
    stmt->arrowSw = true;
    for (auto& arm : sw.cases) {
        CaseArm out;
        out.line = arm.line;
        out.isDefault = arm.isDefault;
        out.nullLabel = arm.nullLabel;
        out.labels = std::move(arm.labels);
        out.patLabels = std::move(arm.patLabels);
        out.patName = arm.patName;
        out.guard = std::move(arm.guard);
        if (arm.value) {
            auto as = std::make_unique<Stmt>();
            as->k = SK::ExprStmt;
            as->line = arm.line;
            auto tgt = std::make_unique<Expr>();
            tgt->k = EK::Ident;
            tgt->name = target;
            auto assign = std::make_unique<Expr>();
            assign->k = EK::Assign;
            assign->op = "=";
            assign->a = std::move(tgt);
            assign->b = std::move(arm.value);
            as->e = std::move(assign);
            out.body.push_back(std::move(as));
        } else {
            for (auto& s : arm.body) {
                if (s->k == SK::Return && s->isYield && s->e) {
                    auto as = std::make_unique<Stmt>();
                    as->k = SK::ExprStmt;
                    as->line = s->line;
                    auto tgt = std::make_unique<Expr>();
                    tgt->k = EK::Ident;
                    tgt->name = target;
                    auto assign = std::make_unique<Expr>();
                    assign->k = EK::Assign;
                    assign->op = "=";
                    assign->a = std::move(tgt);
                    assign->b = std::move(s->e);
                    as->e = std::move(assign);
                    out.body.push_back(std::move(as));
                } else {
                    out.body.push_back(std::move(s));
                }
            }
        }
        stmt->arms.push_back(std::move(out));
    }
    emitStmt(es, *stmt);
}

void emitLocalVar(ES& es, Stmt& st) {
    flushComments(es, st.line);
    bool useVar = st.ty.isVar;
    std::string csT = csTypeStr(es.ctx, st.ty);
    for (size_t i = 0; i < st.names.size(); i++) {
        std::string t = csT;
        for (int k = 0; k < st.dims[i]; k++) t += "[]";
        if (useVar && st.inits[i]) {
            TypeInfo it = inferExprType(es, *st.inits[i]);
            if (it.known && it.dims == 0 && st.dims[i] == 0 && !it.name.empty() &&
                it.name != "object" && it.name != "null") {
                t = it.text();
                useVar = false;
            }
        }
        es.wi();
        if (st.inits[i] && st.inits[i]->k == EK::SwitchE && switchENeedsStmtForm(*st.inits[i])) {
            es.w(t + " " + csIdent(st.names[i]) + ";");
            flushTrailing(es, st.line);
            es.nl();
            if (!useVar && !st.ty.isVar && !st.ty.name.empty()) {
                TypeInfo vt = toInfo(es.ctx, st.ty);
                vt.dims += st.dims[i];
                es.scope.push_back({st.names[i], vt});
            } else {
                es.scope.push_back({st.names[i], {}});
            }
            emitSwitchEAsStmt(es, *st.inits[i], st.names[i]);
            continue;
        }
        if (st.inits[i] && st.inits[i]->k == EK::Lit && st.inits[i]->lt == TT::Null &&
            es.typeParams.count(st.ty.name) > 0) {
            es.w(t + " " + csIdent(st.names[i]) + " = default;");
            flushTrailing(es, st.line);
            es.nl();
            es.scope.push_back({st.names[i], {}});
            continue;
        }
        es.w(t + " " + csIdent(st.names[i]));
        if (st.inits[i]) {
            es.w(" = ");
            TypeInfo expect;
            if (!useVar) {
                expect = toInfo(es.ctx, st.ty);
                expect.dims += st.dims[i];
                expect.known = !st.ty.isVar && !st.ty.name.empty();
            }
            emitExpr(es, *st.inits[i], 0, expect);
        }
        es.w(";");
        flushTrailing(es, st.line);
        es.nl();
        if (!useVar && !st.ty.isVar && !st.ty.name.empty()) {
            TypeInfo vt = toInfo(es.ctx, st.ty);
            vt.dims += st.dims[i];
            es.scope.push_back({st.names[i], vt});
        } else if (st.inits[i]) {
            TypeInfo it = inferExprType(es, *st.inits[i]);
            if (it.known) es.scope.push_back({st.names[i], it});
            else es.scope.push_back({st.names[i], {}});
        } else {
            es.scope.push_back({st.names[i], {}});
        }
    }
}

void emitFor(ES& es, Stmt& st) {
    flushComments(es, st.line);
    es.wi();
    es.w("for (");
    std::vector<VarInfo> pushed;
    if (!st.names.empty()) {
        std::string t = csTypeStr(es.ctx, st.ty);
        es.w(t + " ");
        for (size_t i = 0; i < st.names.size(); i++) {
            if (i) es.w(", ");
            std::string vt = t;
            for (int k = 0; k < st.dims[i]; k++) vt += "[]";
            es.w(csIdent(st.names[i]));
            TypeInfo vi = toInfo(es.ctx, st.ty);
            vi.dims += st.dims[i];
            pushed.push_back({st.names[i], vi});
            if (st.inits[i]) {
                es.w(" = ");
                TypeInfo expect;
                expect = vi;
                emitExpr(es, *st.inits[i], 0, expect);
            }
        }
    } else {
        for (size_t i = 0; i < st.initExprs.size(); i++) {
            if (i) es.w(", ");
            emitExpr(es, *st.initExprs[i], 0);
        }
    }
    es.w(";");
    if (st.e) es.w(" " + captureExpr(es, *st.e, 0));
    es.w(";");
    for (size_t i = 0; i < st.updateExprs.size(); i++) {
        es.w(i == 0 ? " " : ", ");
        emitExpr(es, *st.updateExprs[i], 0);
    }
    es.w(")");
    flushTrailing(es, st.line);
    es.nl();
    for (auto& v : pushed) es.scope.push_back(v);
    emitBodyStmt(es, *st.body);
    for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
}

void emitForeach(ES& es, Stmt& st) {
    flushComments(es, st.line);
    std::string t = csTypeStr(es.ctx, st.ty);
    es.wi();
    es.w("foreach (" + t + " " + csIdent(st.names[0]) + " in " + captureExpr(es, *st.e, 0) + ")");
    flushTrailing(es, st.line);
    es.nl();
    es.scope.push_back({st.names[0], toInfo(es.ctx, st.ty)});
    emitBodyStmt(es, *st.body);
    es.scope.pop_back();
}

void chainInvoke(ES& es, const std::string& tmp, const std::string& vT, Expr& e) {
    std::string m = csIdent(e.name);
    if (e.name == "add" && (vT.rfind("List<", 0) == 0 || vT.rfind("LinkedList<", 0) == 0 ||
                            vT.rfind("IList<", 0) == 0 || vT.rfind("ICollection<", 0) == 0))
        m = "Add";
    else if (e.name == "put" && vT.rfind("Dictionary<", 0) == 0)
        m = "";
    if (!m.empty()) {
        es.w(csIdent(tmp) + "." + m + "(");
        emitArgs(es, e.args);
        es.w(");");
        es.nl();
    }
    if (m.empty()) {
        es.wi();
        es.w(csIdent(tmp) + "[" + captureExprText(es, *e.args[0], 0) + "] = " +
             captureExprText(es, *e.args[1], 0) + ";");
        es.nl();
    }
}

bool emitDictModify(ES& es, Expr& e, const std::string& recv, const TypeInfo& rt,
                    const std::string* chainTmp) {
    if (e.name == "merge" && e.args.size() == 3) {
        std::string tmp = chainTmp ? *chainTmp : "__m" + std::to_string(es.tmpCounter++);
        std::string vT = rt.known && rt.args.size() == 2 ? rt.args[1].text() : "object";
        Expr* fn = e.args[2].get();
        if (chainTmp) es.w(vT + " " + tmp + ";"), es.nl(), es.wi();
        if (fn->k == EK::MRef && fn->obj && fn->obj->k == EK::Ident) {
            std::string wc = mapJavaTypeFqn(fn->obj->name, true);
            std::string wcc = wc.empty() ? fn->obj->name : wc;
            if ((wcc == "int" || wcc == "long" || wcc == "double" || wcc == "float") &&
                (fn->name == "sum" || fn->name == "max" || fn->name == "min")) {
                std::unique_ptr<Expr> val;
                if (fn->name == "sum") {
                    val = std::make_unique<Expr>();
                    val->k = EK::Bin;
                    val->op = "+";
                    val->a = mkIdent(tmp);
                    val->b = cloneExpr(*e.args[1]);
                } else {
                    val = std::make_unique<Expr>();
                    val->k = EK::Call;
                    val->name = fn->name == "max" ? "Max" : "Min";
                    auto mo = std::make_unique<Expr>();
                    mo->k = EK::Ident;
                    mo->name = "Math";
                    val->obj = std::move(mo);
                    val->args.push_back(mkIdent(tmp));
                    val->args.push_back(cloneExpr(*e.args[1]));
                }
                es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
                     ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
                es.nl();
                es.wi();
                es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = ");
                emitExpr(es, *val, 0);
                es.w(";");
                es.nl();
                es.wi();
                es.w("else");
                es.nl();
                es.wi();
                es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
                     captureExpr(es, *e.args[1], 20) + ";");
                es.nl();
                return true;
            }
        }
        if (fn->k == EK::Lambda && !fn->lbodyIsBlock && fn->lparams.size() == 2 &&
            fn->lbody) {
            std::unique_ptr<Expr> val = cloneExpr(*fn->lbody);
            Expr oldVal;
            oldVal.k = EK::Ident;
            oldVal.name = tmp;
            substParamExpr(*val, fn->lparams[0].second, oldVal);
            substParamExpr(*val, fn->lparams[1].second, *e.args[1]);
            es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
                 ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
            es.nl();
            es.wi();
            es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = ");
            emitExpr(es, *val, 0);
            es.w(";");
            es.nl();
            es.wi();
            es.w("else");
            es.nl();
            es.wi();
            es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
                 captureExpr(es, *e.args[1], 20) + ";");
            es.nl();
            return true;
        }
        es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
             ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[2], 20) + "(" + tmp + ", " +
             captureExpr(es, *e.args[1], 20) + ");");
        es.nl();
        es.wi();
        es.w("else");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[1], 20) + ";");
        es.nl();
        return true;
    }
    if (e.name == "computeIfAbsent" && e.args.size() == 2) {
        std::string tmp = chainTmp ? *chainTmp : "__c" + std::to_string(es.tmpCounter++);
        std::string vT = rt.known && rt.args.size() == 2 ? rt.args[1].text() : "object";
        Expr* fn = e.args[1].get();
        if (chainTmp) es.w(vT + " " + tmp + ";"), es.nl(), es.wi();
        if (fn->k == EK::Lambda && !fn->lbodyIsBlock && fn->lparams.size() == 1 &&
            fn->lbody) {
            std::unique_ptr<Expr> val = cloneExpr(*fn->lbody);
            substParamExpr(*val, fn->lparams[0].second, *e.args[0]);
            es.w("if (!" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
                 ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
            es.nl();
            es.wi();
            es.w("{");
            es.nl();
            es.wi();
            es.w("    " + tmp + " = ");
            TypeInfo vti;
            vti.known = true;
            vti.name = vT;
            emitExpr(es, *val, 0, vti);
            es.w(";");
            es.nl();
            es.wi();
            es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " + tmp + ";");
            es.nl();
            es.wi();
            es.w("}");
            es.nl();
            return true;
        }
        es.w("if (!" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
             ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
        es.nl();
        es.wi();
        es.w("{");
        es.nl();
        es.wi();
        std::string kT = rt.known && rt.args.size() == 2 ? rt.args[0].text() : "object";
        es.w("    " + tmp + " = ((System.Func<" + kT + ", " + vT + ">)(" +
             captureExpr(es, *e.args[1], 20) + "))(" + captureExpr(es, *e.args[0], 20) + ");");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " + tmp + ";");
        es.nl();
        es.wi();
        es.w("}");
        es.nl();
        return true;
    }
    if (e.name == "compute" && e.args.size() == 2) {
        std::string tmp = chainTmp ? *chainTmp : "__k" + std::to_string(es.tmpCounter++);
        std::string vT = rt.known && rt.args.size() == 2 ? rt.args[1].text() : "object";
        if (chainTmp) es.w(vT + " " + tmp + ";"), es.nl(), es.wi();
        es.w("if (" + recv + ".TryGetValue(" + captureExpr(es, *e.args[0], 20) +
             ", out " + std::string(chainTmp ? "" : "var ") + tmp + "))");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[1], 20) + "(" + captureExpr(es, *e.args[0], 20) +
             ", " + tmp + ");");
        es.nl();
        es.wi();
        es.w("else");
        es.nl();
        es.wi();
        es.w("    " + recv + "[" + captureExpr(es, *e.args[0], 20) + "] = " +
             captureExpr(es, *e.args[1], 20) + "(" + captureExpr(es, *e.args[0], 20) +
             ", null);");
        es.nl();
        return true;
    }
    return false;
}

bool emitExprStmt(ES& es, Expr& e) {
    if (e.k == EK::Call && e.obj) {
        if (e.obj->k == EK::Call && e.obj->obj) {
            Expr& inner = *e.obj;
            TypeInfo irt = inferExprType(es, *inner.obj);
            if (irt.known && (irt.name == "Dictionary" || irt.name == "SortedDictionary")) {
                std::string recvI = captureExpr(es, *inner.obj, 20);
                std::string tmp = "__t" + std::to_string(es.tmpCounter++);
                if (emitDictModify(es, inner, recvI, irt, &tmp)) {
                    es.wi();
                    chainInvoke(es, tmp, irt.known && irt.args.size() == 2 ? irt.args[1].text() : "object", e);
                    return true;
                }
            }
        }
        TypeInfo rt = inferExprType(es, *e.obj);
        std::string recv = captureExpr(es, *e.obj, 20);
        if (rt.known && (rt.name == "Dictionary" || rt.name == "SortedDictionary")) {
            if (emitDictModify(es, e, recv, rt, nullptr)) return true;
            if (e.name == "putAll" && e.args.size() == 1) {
                std::string tmp = "__p" + std::to_string(es.tmpCounter++);
                es.w("foreach (var " + tmp + " in " + captureExpr(es, *e.args[0], 20) + ")");
                es.nl();
                es.wi();
                es.w("    " + recv + "[" + tmp + ".Key] = " + tmp + ".Value;");
                es.nl();
                return true;
            }
        }
        if (rt.known && rt.name == "StringBuilder") {
            if (e.name == "reverse" && e.args.empty()) {
                es.ctx.needLinq = true;
                es.w(recv + " = new string(" + recv + ".ToString().Reverse().ToArray());");
                es.nl();
                return true;
            }
        }
        if (rt.known && rt.name == "__Scanner") {
            if (e.name == "close" && e.args.empty()) return true;
        }
    }
    emitExpr(es, e, 0);
    return false;
}
const std::set<std::string>& staticReceiverNames() {
    static const std::set<std::string> s = {
        "Math", "StrictMath", "System", "Integer", "Long", "Short", "Byte", "Double",
        "Float", "Boolean", "Character", "String", "Objects", "Arrays", "Collections",
        "Thread", "Optional", "UUID", "Instant", "LocalDate", "LocalTime", "LocalDateTime",
        "Duration", "Period", "Pattern", "Files", "Paths", "Locale", "BigDecimal",
        "BigInteger", "Stream", "IntStream", "LongStream", "DoubleStream", "Collectors",
        "Enum", "Environment", "Console"};
    return s;
}

std::string pathText(const Expr& e) {
    if (e.k == EK::Ident) return e.name;
    if (e.k == EK::Field && e.obj) {
        std::string base = pathText(*e.obj);
        if (base == "__special__") return base;
        return base + "." + e.name;
    }
    return "__special__";
}

bool isStringLit(Expr& e) { return e.k == EK::Lit && (e.lt == TT::Str || e.lt == TT::TBlk); }

std::string stripNullable(const std::string& t) {
    if (endsWith(t, "?")) return t.substr(0, t.size() - 1);
    return t;
}

TypeInfo elemTypeOf(const TypeInfo& t) { return t.elem(); }

TypeInfo inferExprType(ES& es, Expr& e);

TypeInfo inferArgsNone() { return TypeInfo(); }

TypeInfo inferExprType(ES& es, Expr& e) {
    switch (e.k) {
        case EK::Lit: {
            TypeInfo t;
            t.known = true;
            if (e.lt == TT::Str || e.lt == TT::TBlk) t.name = "string";
            else if (e.lt == TT::Chr) t.name = "char";
            else if (e.lt == TT::Bool) t.name = "bool";
            else if (e.lt == TT::Null) t.name = "null";
            else if (e.lt == TT::Int) {
                if (endsWith(e.val, "L")) t.name = "long";
                else t.name = "int";
            } else if (e.lt == TT::Flt) {
                char last = e.val.empty() ? 'd' : e.val.back();
                if (last == 'f' || last == 'F') t.name = "float";
                else t.name = "double";
            }
            return t;
        }
        case EK::Ident: {
            std::string base = firstSeg(e.name);
            for (size_t i = es.scope.size(); i > 0; i--) {
                if (es.scope[i - 1].name == base) {
                    TypeInfo r = es.scope[i - 1].ty;
                    if (e.name.size() > base.size()) r.known = false;
                    return r;
                }
            }
            auto ft = es.ctx.classFieldType.find(es.cls + "." + base);
            if (ft != es.ctx.classFieldType.end()) {
                TypeInfo r = toInfo(es.ctx, ft->second);
                if (e.name.size() > base.size()) r.known = false;
                return r;
            }
            std::string anc = ancestorOf(es.ctx, es.cls);
            int guard = 0;
            while (!anc.empty() && guard++ < 32) {
                auto ft2 = es.ctx.classFieldType.find(anc + "." + base);
                if (ft2 != es.ctx.classFieldType.end()) {
                    TypeInfo r = toInfo(es.ctx, ft2->second);
                    if (e.name.size() > base.size()) r.known = false;
                    return r;
                }
                anc = ancestorOf(es.ctx, anc);
            }
            if (es.ctx.enumConstOwner.count(base)) {
                TypeInfo t;
                t.known = true;
                t.name = es.ctx.enumConstOwner[base];
                return t;
            }
            if (es.ctx.staticImports.count(e.name)) return TypeInfo();
            if (staticReceiverNames().count(base) || es.ctx.typeByName.count(base)) {
                if (e.name == base) {
                    TypeInfo t;
                    t.known = true;
                    t.name = base;
                    return t;
                }
            }
            if (base == "System" && e.name == "System.out") {
                TypeInfo t;
                t.known = true;
                t.name = "__consoleOut";
                return t;
            }
            if (base == "System" && e.name == "System.err") {
                TypeInfo t;
                t.known = true;
                t.name = "__consoleErr";
                return t;
            }
            return TypeInfo();
        }
        case EK::This: {
            TypeInfo t;
            t.known = true;
            t.name = es.cls;
            return t;
        }
        case EK::Field: {
            if (!e.obj) return TypeInfo();
            if (e.name == "this") {
                TypeInfo t;
                t.known = true;
                t.name = firstSeg(pathText(*e.obj));
                return t;
            }
            TypeInfo ot = inferExprType(es, *e.obj);
            if (!ot.known) return TypeInfo();
            if (ot.dims > 0 && e.name == "length") {
                TypeInfo t;
                t.known = true;
                t.name = "int";
                return t;
            }
            if (ot.name == "string" && e.name == "Length") {
                TypeInfo t;
                t.known = true;
                t.name = "int";
                return t;
            }
            if (ot.name == "Math" && (e.name == "PI" || e.name == "E")) {
                TypeInfo t;
                t.known = true;
                t.name = "double";
                return t;
            }
            if (isNumericCs(stripNullable(ot.name)) && endsWith(ot.name, "?")) {
                if (e.name == "Value" || e.name == "HasValue") {
                    TypeInfo t;
                    t.known = true;
                    t.name = e.name == "Value" ? stripNullable(ot.name) : "bool";
                    return t;
                }
            }
            if (ot.name == "DateTimeOffset") {
                if (e.name == "ToUnixTimeMilliseconds" || e.name == "ToUnixTimeSeconds") return TypeInfo();
            }
            auto ft = es.ctx.classFieldType.find(ot.name + "." + e.name);
            if (ft != es.ctx.classFieldType.end()) return toInfo(es.ctx, ft->second);
            std::string anc = ancestorOf(es.ctx, ot.name);
            int guard2 = 0;
            while (!anc.empty() && guard2++ < 32) {
                auto ft2 = es.ctx.classFieldType.find(anc + "." + e.name);
                if (ft2 != es.ctx.classFieldType.end()) return toInfo(es.ctx, ft2->second);
                anc = ancestorOf(es.ctx, anc);
            }
            if (es.ctx.enumClassTypes.count(ot.name)) {
                if (e.name == "Ordinal") {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "Name") {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
            }
            return TypeInfo();
        }
        case EK::Call: {
            TypeInfo ot;
            if (e.obj) ot = inferExprType(es, *e.obj);
            std::string staticOwner = e.obj ? lastSeg(pathText(*e.obj)) : "";
            if (e.obj && (e.obj->k == EK::Ident || e.obj->k == EK::Field)) {
                if (staticOwner == "List" || staticOwner == "ArrayList") {
                    if (e.name == "of") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "List";
                        if (!e.args.empty()) {
                            TypeInfo element = inferExprType(es, *e.args.front());
                            if (element.known) t.args.push_back(element);
                        }
                        return t;
                    }
                }
                if (staticOwner == "Set" || staticOwner == "HashSet") {
                    if (e.name == "of") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "HashSet";
                        if (!e.args.empty()) {
                            TypeInfo element = inferExprType(es, *e.args.front());
                            if (element.known) t.args.push_back(element);
                        }
                        return t;
                    }
                }
                if (staticOwner == "Thread") {
                    if (e.name == "currentThread") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "Thread";
                        return t;
                    }
                }
                if (staticOwner == "Map" || staticOwner == "HashMap") {
                    if (e.name == "of") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "Dictionary";
                        if (e.args.size() >= 2) {
                            TypeInfo key = inferExprType(es, *e.args[0]);
                            TypeInfo value = inferExprType(es, *e.args[1]);
                            if (key.known) t.args.push_back(key);
                            if (value.known) t.args.push_back(value);
                        }
                        return t;
                    }
                }
                if (staticOwner == "Stream" || staticOwner == "IntStream" ||
                    staticOwner == "LongStream" || staticOwner == "DoubleStream") {
                    if (e.name == "of" || e.name == "empty" || e.name == "range" ||
                        e.name == "rangeClosed") {
                        TypeInfo t;
                        t.known = true;
                        t.name = "IEnumerable";
                        TypeInfo element;
                        if (staticOwner == "IntStream") {
                            element.known = true;
                            element.name = "int";
                        } else if (staticOwner == "LongStream") {
                            element.known = true;
                            element.name = "long";
                        } else if (staticOwner == "DoubleStream") {
                            element.known = true;
                            element.name = "double";
                        } else if (!e.targs.empty()) {
                            element = toInfo(es.ctx, e.targs.front());
                        } else if (!e.args.empty()) {
                            element = inferExprType(es, *e.args.front());
                        }
                        if (!element.known) {
                            element.known = true;
                            element.name = "object";
                        }
                        t.args.push_back(element);
                        return t;
                    }
                }
            }
            if (e.name == "stream") {
                TypeInfo t = ot;
                if (t.known) t.name = "IEnumerable";
                return t;
            }
            if (e.name == "boxed") {
                TypeInfo t = ot;
                if (t.known) t.name = "IEnumerable";
                return t;
            }
            if (e.name == "entrySet" && ot.known &&
                (ot.name == "Dictionary" || ot.name == "SortedDictionary")) {
                TypeInfo t = ot;
                t.name = "IEnumerable";
                t.args.clear();
                return t;
            }
            if (ot.known) {
                std::string base = stripNullable(ot.name);
                auto& rets = es.ctx.methodReturns[base];
                auto it = rets.find(e.name + "/" + std::to_string(e.args.size()));
                if (it != rets.end() && !it->second.empty()) return toInfo(es.ctx, it->second);
                std::string anc = ancestorOf(es.ctx, base);
                int guard = 0;
                while (!anc.empty() && guard++ < 32) {
                    auto& rets2 = es.ctx.methodReturns[anc];
                    auto it2 = rets2.find(e.name + "/" + std::to_string(e.args.size()));
                    if (it2 != rets2.end() && !it2->second.empty())
                        return toInfo(es.ctx, it2->second);
                    anc = ancestorOf(es.ctx, anc);
                }
                if (e.name == "get" && ot.args.size() == 1 &&
                    (ot.name == "List" || ot.name == "IList" || ot.name == "LinkedList"))
                    return ot.args[0];
                if (e.name == "get" && ot.args.size() == 2 &&
                    (ot.name == "Dictionary" || ot.name == "SortedDictionary"))
                    return ot.args[1];
                if (e.name == "Values" && es.ctx.enumClassTypes.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = ot.name;
                    t.dims = 1;
                    return t;
                }
                if (e.name == "values" &&
                    (es.ctx.simpleEnums.count(ot.name) || es.ctx.enumClassTypes.count(ot.name))) {
                    TypeInfo t = ot;
                    t.dims = 1;
                    return t;
                }
                if (e.name == "keySet" && ot.args.size() == 2) return ot.args[0];
                if (e.name == "values" && ot.args.size() == 2) return ot.args[1];
                if (e.name == "entrySet" && ot.args.size() == 2) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "IEnumerable";
                    return t;
                }
                if (e.name == "getOrDefault" && ot.args.size() == 2) return ot.args[1];
                if (e.name == "toString" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "getClass" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "Type";
                    return t;
                }
                if (e.name == "ordinal" && e.args.empty() && es.ctx.simpleEnums.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "ordinal" && e.args.empty() && es.ctx.enumClassTypes.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "name" && e.args.empty() && es.ctx.simpleEnums.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "name" && e.args.empty() && es.ctx.enumClassTypes.count(ot.name)) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "doubleValue" || e.name == "floatValue") {
                    TypeInfo t;
                    t.known = true;
                    t.name = endsWith(ot.name, "?") ? stripNullable(ot.name) : ot.name;
                    return t;
                }
                if (endsWith(ot.name, "?") && e.name == "Value") {
                    TypeInfo t;
                    t.known = true;
                    t.name = stripNullable(ot.name);
                    return t;
                }
                if (e.name == "hasNext" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "bool";
                    return t;
                }
                if (e.name == "next" && e.args.empty() && !ot.args.empty()) return ot.args[0];
                if (e.name == "Current" && !ot.args.empty()) return ot.args[0];
                if (e.name == "append" && ot.name == "StringBuilder") {
                    TypeInfo t;
                    t.known = true;
                    t.name = "StringBuilder";
                    return t;
                }
                if (e.name == "ToString" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "getMessage" && e.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "string";
                    return t;
                }
                if (e.name == "orElse" && e.args.size() == 1 && endsWith(ot.name, "?")) {
                    TypeInfo t;
                    t.known = true;
                    t.name = stripNullable(ot.name);
                    t.args = ot.args;
                    return t;
                }
                static const std::set<std::string> linqKeep = {
                    "Where", "Select", "SelectMany", "OrderBy", "Take", "Skip",
                    "Distinct", "Concat", "Reverse", "OfType", "Cast",
                    "filter", "map", "mapToInt", "mapToLong", "mapToDouble",
                    "mapToObj", "flatMap", "distinct", "sorted", "limit", "skip",
                    "peek"};
                if (linqKeep.count(e.name) && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "IEnumerable";
                    t.args = ot.args;
                    if ((e.name == "map" || e.name == "mapToInt" || e.name == "mapToLong" ||
                         e.name == "mapToDouble" || e.name == "mapToObj") && !e.args.empty()) {
                        TypeInfo mapped;
                        if (e.name == "mapToInt") {
                            mapped.known = true;
                            mapped.name = "int";
                        } else if (e.name == "mapToLong") {
                            mapped.known = true;
                            mapped.name = "long";
                        } else if (e.name == "mapToDouble") {
                            mapped.known = true;
                            mapped.name = "double";
                        } else if (e.args[0]->k == EK::Lambda && e.args[0]->lbody) {
                            mapped = inferExprType(es, *e.args[0]->lbody);
                        }
                        if (mapped.known) t.args[0] = mapped;
                    } else if (e.name == "flatMap" && !e.args.empty() &&
                               e.args[0]->k == EK::Lambda && e.args[0]->lbody) {
                        TypeInfo mapped = inferExprType(es, *e.args[0]->lbody);
                        if (mapped.known && mapped.name == "IEnumerable" && !mapped.args.empty())
                            t.args[0] = mapped.args[0];
                    }
                    return t;
                }
                if (e.name == "ToList" && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "List";
                    t.args = ot.args;
                    return t;
                }
                if (e.name == "ToHashSet" && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "HashSet";
                    t.args = ot.args;
                    return t;
                }
                if (e.name == "ToArray" && !ot.args.empty()) {
                    TypeInfo t = ot.args[0];
                    t.dims += 1;
                    return t;
                }
                if (e.name == "Count" && e.args.empty() && !ot.args.empty()) {
                    TypeInfo t;
                    t.known = true;
                    t.name = "int";
                    return t;
                }
                if (e.name == "First" || e.name == "Last" || e.name == "FirstOrDefault" ||
                    e.name == "LastOrDefault" || e.name == "findFirst" || e.name == "findAny") {
                    if (!ot.args.empty()) {
                        TypeInfo t = ot.args[0];
                        if (!t.name.empty() && !endsWith(t.name, "?") && !t.name.empty() &&
                            t.name != "int" && t.name != "long" && t.name != "double" &&
                            t.name != "float" && t.name != "bool" && t.name != "char")
                            t.name += "?";
                        return t;
                    }
                }
            }
            if (!e.obj) {
                std::string base = firstSeg(e.name);
                auto& rets = es.ctx.methodReturns[es.cls];
                auto it = rets.find(e.name + "/" + std::to_string(e.args.size()));
                if (it != rets.end() && !it->second.empty()) return toInfo(es.ctx, it->second);
            }
            return TypeInfo();
        }
        case EK::New: {
            TypeInfo t = toInfo(es.ctx, e.ty);
            return t;
        }
        case EK::NewArr: {
            TypeInfo t = toInfo(es.ctx, e.ty);
            if (t.known) t.dims += (int)e.dims.size();
            return t;
        }
        case EK::ArrIdx: {
            if (!e.a) return TypeInfo();
            TypeInfo bt = inferExprType(es, *e.a);
            if (!bt.known) return TypeInfo();
            if (bt.dims > 0) {
                TypeInfo t = bt;
                t.dims--;
                return t;
            }
            if (!bt.args.empty()) {
                if ((bt.name == "Dictionary" || bt.name == "SortedDictionary") &&
                    bt.args.size() == 2)
                    return bt.args[1];
                return bt.args[0];
            }
            return TypeInfo();
        }
        case EK::Bin: {
            if (e.op == "&&" || e.op == "||" || e.op == "==" || e.op == "!=" || e.op == "<" ||
                e.op == ">" || e.op == "<=" || e.op == ">=") {
                TypeInfo t;
                t.known = true;
                t.name = "bool";
                return t;
            }
            TypeInfo a = e.a ? inferExprType(es, *e.a) : TypeInfo();
            TypeInfo b = e.b ? inferExprType(es, *e.b) : TypeInfo();
            if (a.known && a.name == "string") return a;
            if (b.known && b.name == "string") return b;
            if (e.op == "+" && a.known && (a.name == "string")) return a;
            if (a.known && b.known) {
                int ra = csRank(a.name), rb = csRank(b.name);
                if (ra > 0 && rb > 0) {
                    TypeInfo t;
                    t.known = true;
                    t.name = ra >= rb ? a.name : b.name;
                    if (ra == 3 && rb == 3 && (a.name == "char" || b.name == "char")) t.name = "int";
                    return t;
                }
            }
            return a.known ? a : b;
        }
        case EK::Un: {
            if (e.op == "!") {
                TypeInfo t;
                t.known = true;
                t.name = "bool";
                return t;
            }
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::Post: {
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::Assign: {
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::Ternary: {
            TypeInfo a = e.b ? inferExprType(es, *e.b) : TypeInfo();
            if (a.known) return a;
            return e.c ? inferExprType(es, *e.c) : TypeInfo();
        }
        case EK::Cast: {
            TypeInfo t = toInfo(es.ctx, e.ty);
            return t;
        }
        case EK::InstOf: {
            TypeInfo t;
            t.known = true;
            t.name = "bool";
            return t;
        }
        case EK::Paren: {
            return e.a ? inferExprType(es, *e.a) : TypeInfo();
        }
        case EK::SwitchE: {
            for (auto& arm : e.cases) {
                if (arm.value) return inferExprType(es, *arm.value);
                if (arm.isBlockVal && !arm.body.empty()) {
                    for (auto& s : arm.body)
                        if (s->k == SK::Return && s->e) return inferExprType(es, *s->e);
                }
            }
            return TypeInfo();
        }
        case EK::CtorCall:
        case EK::Lambda:
        case EK::MRef:
        case EK::ClassLit:
        default:
            return TypeInfo();
    }
}
const int P_BITOR = 3, P_XOR = 4, P_BITAND = 5, P_EQ = 6, P_REL = 7,
          P_ADD = 9, P_MUL = 10, P_UNARY = 11, P_POST = 12, P_PRIMARY = 13;

void emitExpr(ES& es, Expr& e, int prec, const TypeInfo& expect);
void emitCall(ES& es, Expr& e, const TypeInfo& expect);

void emitArgs(ES& es, std::vector<std::unique_ptr<Expr>>& args) {
    for (size_t i = 0; i < args.size(); i++) {
        if (i) es.w(", ");
        emitExpr(es, *args[i], 0);
    }
}

void emitFmtArg(ES& es, Expr& e, int tag) {
    if (tag == 1) {
        TypeInfo at = inferExprType(es, e);
        if (at.known && at.name == "bool") {
            es.w("(");
            emitExpr(es, e, 0);
            es.w(" ? \"true\" : \"false\")");
            return;
        }
    } else if (tag == 2) {
        es.w("Convert.ToString(");
        emitExpr(es, e, 0);
        es.w(", 8)");
        return;
    }
    emitExpr(es, e, 0);
}

std::string captureExprText(ES& es, Expr& e, int prec, const TypeInfo& expect) {
    std::ostringstream sub;
    std::ostream* saved = es.cur;
    es.cur = &sub;
    emitExpr(es, e, prec, expect);
    es.cur = saved;
    return sub.str();
}

void emitLiteral(ES& es, Expr& e, const TypeInfo& expect) {
    switch (e.lt) {
        case TT::Str:
            es.w("\"" + escapeCsStr(e.val) + "\"");
            break;
        case TT::TBlk: {
            es.w("\"" + escapeCsStr(e.val) + "\"");
            break;
        }
        case TT::Chr:
            es.w("'" + escapeCsChar(e.val) + "'");
            break;
        case TT::Null:
            es.w("null");
            break;
        case TT::Bool:
            es.w(e.val);
            break;
        case TT::Int:
        case TT::Flt:
            es.w(e.val);
            break;
        default:
            es.w(e.val);
            break;
    }
    (void)expect;
}

void emitIdent(ES& es, Expr& e) {
    std::string name = e.name;
    if (name == "super") {
        es.w("base");
        return;
    }
    if (name == "this") {
        es.w("this");
        return;
    }
    if (name == "System.out") {
        es.w("Console.Out");
        return;
    }
    if (name == "System.err") {
        es.w("Console.Error");
        return;
    }
    if (name == "System.in") {
        es.w("Console.In");
        return;
    }
    if (es.ctx.staticImports.count(name)) {
        es.w(es.ctx.staticImports[name]);
        return;
    }
    if (es.ctx.needStaticMath) {
        if (name == "PI") { es.w("Math.PI"); return; }
        if (name == "E") { es.w("Math.E"); return; }
        std::string mn = staticMathName(name);
        if (mn != name) {
            es.w("Math." + mn);
            return;
        }
    }
    std::string seg = firstSeg(name);
    if (seg == name) {
        std::string mapped = mapJavaTypeFqn(name, true);
        if (!mapped.empty() && name != "var") {
            es.w(mapped);
            return;
        }
    }
    es.w(csIdent(name));
}

void emitFieldExpr(ES& es, Expr& e) {
    if (e.name == "this" && e.obj) {
        es.w("this");
        return;
    }
    if (!e.obj) {
        es.w(e.name);
        return;
    }
    std::string path = pathText(e);
    if (path == "System.out") {
        es.w("Console.Out");
        return;
    }
    if (path == "System.err") {
        es.w("Console.Error");
        return;
    }
    if (path == "System.in") {
        es.w("Console.In");
        return;
    }
    TypeInfo ot = inferExprType(es, *e.obj);
    std::string recv = captureExprText(es, *e.obj, P_POST);
    if (ot.known) {
        auto rcit = es.ctx.recordComps.find(ot.name);
        if (rcit != es.ctx.recordComps.end()) {
            for (auto& cn : rcit->second) {
                if (cn == e.name) {
                    es.w(recv + "." + pascalCase(e.name));
                    return;
                }
            }
        }
        if (ot.dims > 0 && e.name == "length") {
            es.w(recv + ".Length");
            return;
        }
        if (ot.name == "string" && e.name == "length") {
            es.w(recv + ".Length");
            return;
        }
        if (ot.name == "Math" && e.name == "PI") {
            es.w("Math.PI");
            return;
        }
        if (ot.name == "Math" && e.name == "E") {
            es.w("Math.E");
            return;
        }
        static const std::map<std::string, std::string> wrapperFields = {
            {"MAX_VALUE", "MaxValue"}, {"MIN_VALUE", "MinValue"},
            {"POSITIVE_INFINITY", "PositiveInfinity"}, {"NEGATIVE_INFINITY", "NegativeInfinity"},
            {"NaN", "NaN"}, {"SIZE", "Size"}, {"BYTES", "Bytes"}, {"TYPE", "Type"}};
        if (isNumericCs(stripNullable(ot.name)) || ot.name == "bool") {
            auto wf = wrapperFields.find(e.name);
            if (wf != wrapperFields.end()) {
                if (wf->second == "Size" || wf->second == "Bytes") {
                    static const std::map<std::string, std::string> bits = {
                        {"byte", "8"}, {"sbyte", "8"}, {"short", "16"}, {"ushort", "16"},
                        {"int", "32"}, {"uint", "32"}, {"long", "64"}, {"ulong", "64"},
                        {"char", "16"}, {"float", "32"}, {"double", "64"}};
                    auto bt = bits.find(stripNullable(ot.name));
                    if (bt != bits.end()) {
                        es.w(bt->second);
                        return;
                    }
                }
                if (wf->second == "Type") {
                    es.w("typeof(" + stripNullable(ot.name) + ")");
                    return;
                }
                if (wf->second != "Size" && wf->second != "Bytes" && wf->second != "Type") {
                    es.w(stripNullable(ot.name) + "." + wf->second);
                    return;
                }
            }
        }
        if (ot.name == "BigInteger") {
            static const std::map<std::string, std::string> biConst = {
                {"ONE", "BigInteger.One"}, {"ZERO", "BigInteger.Zero"},
                {"TEN", "new BigInteger(10)"}, {"TWO", "new BigInteger(2)"},
                {"MINUS_ONE", "BigInteger.MinusOne"}};
            auto bc = biConst.find(e.name);
            if (bc != biConst.end()) {
                es.ctx.needNumerics = true;
                es.w(bc->second);
                return;
            }
        }
        if (es.ctx.enumClassTypes.count(ot.name)) {
            if (e.name == "Ordinal") {
                es.w(recv + ".Ordinal");
                return;
            }
            if (e.name == "Name") {
                es.w(recv + ".Name");
                return;
            }
        }
        if (es.ctx.simpleEnums.count(ot.name)) {
            if (e.name == "ordinal") {
                es.w("(int)" + recv);
                return;
            }
        }
    }
    if (ot.known && ot.name == "System" ) {
        static const std::map<std::string, std::string> sysFields = {
            {"in", "Console.In"}, {"out", "Console.Out"}, {"err", "Console.Error"}};
        auto sf = sysFields.find(e.name);
        if (sf != sysFields.end()) {
            es.w(sf->second);
            return;
        }
    }
    if (e.obj->k == EK::Ident) {
        std::string base = e.obj->name;
        std::string mappedBase = mapJavaTypeFqn(base, true);
        if (mappedBase.empty()) mappedBase = base;
        if (mappedBase == "Integer" || mappedBase == "int") {
            if (e.name == "MAX_VALUE") { es.w("int.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("int.MinValue"); return; }
        }
        if (mappedBase == "Long" || mappedBase == "long") {
            if (e.name == "MAX_VALUE") { es.w("long.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("long.MinValue"); return; }
        }
        if (mappedBase == "Double" || mappedBase == "double") {
            if (e.name == "MAX_VALUE") { es.w("double.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("double.MinValue"); return; }
            if (e.name == "NaN") { es.w("double.NaN"); return; }
            if (e.name == "POSITIVE_INFINITY") { es.w("double.PositiveInfinity"); return; }
            if (e.name == "NEGATIVE_INFINITY") { es.w("double.NegativeInfinity"); return; }
        }
        if (mappedBase == "Float" || mappedBase == "float") {
            if (e.name == "MAX_VALUE") { es.w("float.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("float.MinValue"); return; }
        }
        if (mappedBase == "Character" || mappedBase == "char") {
            if (e.name == "MAX_VALUE") { es.w("char.MaxValue"); return; }
            if (e.name == "MIN_VALUE") { es.w("char.MinValue"); return; }
        }
        if (mappedBase == "StandardCharsets") {
            if (e.name == "UTF_8") { es.ctx.needText = true; es.w("Encoding.UTF8"); return; }
            if (e.name == "UTF_16") { es.ctx.needText = true; es.w("Encoding.Unicode"); return; }
            if (e.name == "UTF_16BE") { es.ctx.needText = true; es.w("Encoding.BigEndianUnicode"); return; }
            if (e.name == "ISO_8859_1") { es.ctx.needText = true; es.w("Encoding.Latin1"); return; }
            if (e.name == "US_ASCII") { es.ctx.needText = true; es.w("Encoding.ASCII"); return; }
        }
        if (mappedBase == "Charset") {
            if (e.name == "forName") return;
        }
    }
    emitExpr(es, *e.obj, P_POST);
    es.w("." + csIdent(e.name));
}

void emitAssignExpr(ES& es, Expr& e, int prec) {
    TypeInfo targetType;
    if (e.a) targetType = inferExprType(es, *e.a);
    if (prec > 0) es.w("(");
    if (e.a) emitExpr(es, *e.a, P_PRIMARY);
    es.w(" " + e.op + " ");
    if (e.b) emitExpr(es, *e.b, 0, targetType);
    if (prec > 0) es.w(")");
    (void)prec;
}

void emitTernaryExpr(ES& es, Expr& e, int prec) {
    if (prec > 0) es.w("(");
    if (e.a) emitExpr(es, *e.a, 0);
    es.w(" ? ");
    TypeInfo tb = e.b ? inferExprType(es, *e.b) : TypeInfo();
    TypeInfo tc = e.c ? inferExprType(es, *e.c) : TypeInfo();
    bool bNullable = tb.known && endsWith(tb.name, "?");
    bool cNullable = tc.known && endsWith(tc.name, "?");
    if (e.b) {
        if (e.b->k == EK::Lit && e.b->lt == TT::Null &&
            ((tc.known && es.typeParams.count(tc.name) > 0) ||
             (tb.known && es.typeParams.count(tb.name) > 0))) {
            es.w("default");
        } else if (bNullable && !cNullable && tc.known && tc.name != "null" && tc.name != "object") {
            es.w("(");
            emitExpr(es, *e.b, P_POST);
            es.w(".Value)");
        } else {
            emitExpr(es, *e.b, 0);
        }
    }
    es.w(" : ");
    if (e.c) {
        if (e.c->k == EK::Lit && e.c->lt == TT::Null && tb.known &&
            es.typeParams.count(tb.name) > 0) {
            es.w("default");
        } else if (cNullable && !bNullable && tb.known && tb.name != "null" && tb.name != "object") {
            es.w("(");
            emitExpr(es, *e.c, P_POST);
            es.w(".Value)");
        } else {
            emitExpr(es, *e.c, 0);
        }
    }
    if (prec > 0) es.w(")");
}

void emitBinExpr(ES& es, Expr& e, int prec) {
    int myPrec = binPrec(e.op);
    bool paren = myPrec < prec;
    if (paren) es.w("(");
    if (e.a) {
        TypeInfo ta = inferExprType(es, *e.a);
        TypeInfo tb = e.b ? inferExprType(es, *e.b) : TypeInfo();
        bool strCtx = (tb.known && tb.name == "string") || (ta.known && ta.name == "string");
        if (e.op == "+" && ta.known && ta.name == "bool" && (strCtx || (tb.known && tb.name == "bool"))) {
            es.w("(");
            emitExpr(es, *e.a, P_EQ);
            es.w(" ? \"true\" : \"false\")");
        } else {
            emitExpr(es, *e.a, myPrec);
        }
    }
    es.w(" " + e.op + " ");
    if (e.b) {
        TypeInfo ta = e.a ? inferExprType(es, *e.a) : TypeInfo();
        TypeInfo tb = inferExprType(es, *e.b);
        bool strCtx = ta.known && ta.name == "string";
        if (e.op == "+" && tb.known && tb.name == "bool" && (strCtx || (ta.known && ta.name == "bool"))) {
            es.w("(");
            emitExpr(es, *e.b, P_EQ);
            es.w(" ? \"true\" : \"false\")");
        } else {
            emitExpr(es, *e.b, myPrec + 1);
        }
    }
    if (paren) es.w(")");
}

void emitUnExpr(ES& es, Expr& e, int prec) {
    bool paren = P_UNARY < prec;
    if (paren) es.w("(");
    es.w(e.op);
    if (e.a) emitExpr(es, *e.a, P_UNARY);
    if (paren) es.w(")");
}

void emitPostExpr(ES& es, Expr& e, int prec) {
    bool paren = P_POST < prec;
    if (paren) es.w("(");
    if (e.a) emitExpr(es, *e.a, P_POST);
    es.w(e.op);
    if (paren) es.w(")");
}

void emitCastExpr(ES& es, Expr& e, int prec, const TypeInfo& expect) {
    std::string t = csTypeStr(es.ctx, e.ty);
    std::string prim = stripNullable(t);
    if (expect.known && prim != t && expect.name == prim) t = prim;
    bool paren = P_UNARY < prec;
    if (paren) es.w("(");
    es.w("(" + t + ")");
    if (e.a) emitExpr(es, *e.a, P_POST);
    if (paren) es.w(")");
}

void emitInstOfExpr(ES& es, Expr& e, int prec) {
    bool paren = P_REL < prec;
    if (paren) es.w("(");
    if (e.a) emitExpr(es, *e.a, P_REL);
    es.w(" is " + csTypeStr(es.ctx, e.ty, true));
    if (!e.patName.empty()) es.w(" " + e.patName);
    else if (!e.patComps.empty()) {
        es.w("(");
        for (size_t i = 0; i < e.patComps.size(); i++) {
            if (i) es.w(", ");
            es.w(csTypeStr(es.ctx, e.patComps[i].first, true) + " " + e.patComps[i].second);
        }
        es.w(")");
    }
    if (paren) es.w(")");
}

void emitNewExpr(ES& es, Expr& e, const TypeInfo& expect) {
    if (e.anon) {
        es.w("new " + e.ty.name);
        return;
    }
    std::string mapped = csTypeStr(es.ctx, e.ty, true);
    if (mapped == "BigInteger" && e.args.size() == 1 &&
        inferExprType(es, *e.args[0]).name == "string") {
        es.w("BigInteger.Parse(");
        emitExpr(es, *e.args[0], 0);
        es.w(")");
        return;
    }
    if ((mapped == "decimal" || mapped == "BigDecimal") && e.args.size() == 1 &&
        inferExprType(es, *e.args[0]).name == "string") {
        es.ctx.needGlobalization = true;
        es.w("decimal.Parse(");
        emitExpr(es, *e.args[0], 0);
        es.w(", System.Globalization.CultureInfo.InvariantCulture)");
        return;
    }
    if (mapped == "__Scanner" && e.args.size() == 1 &&
        inferExprType(es, *e.args[0]).name == "string") {
        es.w("new __Scanner(new System.IO.StringReader(");
        emitExpr(es, *e.args[0], 0);
        es.w("))");
        return;
    }
    es.w("new ");
    if (e.ty.diamond) {
        bool expOk = expect.known && expect.name != "void" && expect.name != "null";
        if (expOk && !expect.args.empty()) {
            std::string tn = expect.name;
            es.w(tn + "<");
            for (size_t i = 0; i < expect.args.size(); i++) {
                if (i) es.w(", ");
                es.w(expect.args[i].text());
            }
            es.w(">");
        } else if (expOk) {
            es.w(expect.name);
        } else {
            if (mapped == "List" || mapped == "HashSet") es.w(mapped + "<object>");
            else if (mapped == "Dictionary") es.w("Dictionary<object, object>");
            else if (mapped == "SortedDictionary") es.w("SortedDictionary<object, object>");
            else if (mapped == "SortedSet") es.w("SortedSet<object>");
            else es.w(mapped);
        }
    } else {
        es.w(mapped);
    }
    es.w("(");
    emitArgs(es, e.args);
    es.w(")");
}

void emitNewArrExpr(ES& es, Expr& e, const TypeInfo& expect) {
    TypeRef elementType = e.ty;
    int trailingDims = elementType.dims;
    elementType.dims = 0;
    std::string base = csTypeStr(es.ctx, elementType, true);
    if (e.implArr && e.args.empty() && e.dims.empty()) {
        if (expect.known && expect.dims > 0) {
            TypeInfo element = expect;
            element.dims--;
            es.w("new " + element.text() + "[0]");
        } else {
            es.w("new object[0]");
        }
        return;
    }
    if (e.implArr && !e.args.empty() && e.ty.name.empty()) {
        if (expect.known && expect.dims >= 1) {
            es.w("new " + expect.text() + " { ");
            TypeInfo et = expect;
            et.dims--;
            for (size_t i = 0; i < e.args.size(); i++) {
                if (i) es.w(", ");
                emitExpr(es, *e.args[i], 0, et);
            }
            es.w(" }");
            return;
        }
        es.w("{ ");
        for (size_t i = 0; i < e.args.size(); i++) {
            if (i) es.w(", ");
            TypeInfo et2 = expect.known && !expect.args.empty() ? expect.args[0] : TypeInfo();
            emitExpr(es, *e.args[i], 0, et2);
        }
        es.w(" }");
        return;
    }
    if (!e.dims.empty() && e.dims.size() >= 2) {
        es.ctx.needLinq = true;
        for (size_t i = 0; i + 1 < e.dims.size(); i++) {
            es.w("Enumerable.Range(0, ");
            emitExpr(es, *e.dims[i], 0);
            es.w(").Select(__jdim" + std::to_string(i) + " => ");
        }
        es.w("new " + base + "[");
        emitExpr(es, *e.dims.back(), 0);
        es.w("]");
        for (int i = 0; i < trailingDims; i++) es.w("[]");
        for (size_t i = 1; i < e.dims.size(); i++) es.w(").ToArray()");
        return;
    }
    if (!e.dims.empty()) {
        es.w("new " + base + "[");
        emitExpr(es, *e.dims[0], 0);
        es.w("]");
        for (int i = 0; i < trailingDims; i++) es.w("[]");
        return;
    }
    if (!e.args.empty()) {
        es.w("new " + base);
        for (int i = 0; i < trailingDims; i++) es.w("[]");
        es.w(" { ");
        for (size_t i = 0; i < e.args.size(); i++) {
            if (i) es.w(", ");
            TypeInfo et = toInfo(es.ctx, e.ty);
            et.dims -= 1;
            et.known = !e.ty.name.empty();
            emitExpr(es, *e.args[i], 0, et);
        }
        es.w(" }");
        return;
    }
    es.w("new " + base);
    for (int i = 0; i < trailingDims; i++) es.w("[]");
}

void emitLambdaExpr(ES& es, Expr& e) {
    es.w("(");
    for (size_t i = 0; i < e.lparams.size(); i++) {
        if (i) es.w(", ");
        if (e.lparams[i].first.isVar) es.w(csIdent(e.lparams[i].second));
        else if (!e.lparams[i].first.empty()) {
            es.w(csTypeStr(es.ctx, e.lparams[i].first) + " " + csIdent(e.lparams[i].second));
        } else es.w(csIdent(e.lparams[i].second));
    }
    es.w(") => ");
    if (e.lbodyIsBlock) {
        es.w("{");
        es.nl();
        es.indent++;
        std::vector<VarInfo> pushed;
        for (auto& lp : e.lparams) {
            VarInfo vi;
            vi.name = lp.second;
            if (!lp.first.empty() && !lp.first.isVar) vi.ty = toInfo(es.ctx, lp.first);
            es.scope.push_back(vi);
            pushed.push_back(vi);
        }
        for (auto& s : e.lblock) emitStmt(es, *s);
        for (size_t i = 0; i < pushed.size(); i++) es.scope.pop_back();
        es.indent--;
        es.wi();
        es.w("}");
    } else {
        if (e.lbody) emitExpr(es, *e.lbody, 0);
    }
}

void emitMRefExpr(ES& es, Expr& e, const TypeInfo& expect) {
    std::string path = e.obj ? pathText(*e.obj) : "";
    if (path == "System.out") {
        es.w("Console.WriteLine");
        return;
    }
    if (path == "System.err") {
        es.w("Console.Error.WriteLine");
        return;
    }
    if (e.name == "new") {
        TypeInfo dgt = expect;
        if (dgt.known && startsWith(dgt.name, "Func") && dgt.args.size() >= 1) {
            size_t n = dgt.args.size() - 1;
            es.w("(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(") => new ");
            emitExpr(es, *e.obj, P_PRIMARY);
            es.w("(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(")");
            return;
        }
        es.w("__m0 => new ");
        emitExpr(es, *e.obj, P_PRIMARY);
        es.w("(__m0)");
        return;
    }
    if (e.obj && e.obj->k == EK::Ident) {
        std::string tn = e.obj->name;
        std::string cn = mapJavaTypeFqn(tn, true);
        std::string cc = cn.empty() ? tn : cn;
        std::string staticArity;
        {
            const std::string prefix = tn + "." + e.name + "/";
            for (const auto& sm : es.ctx.staticMethods) {
                if (startsWith(sm, prefix)) {
                    staticArity = sm.substr(prefix.size());
                    break;
                }
            }
        }
        if (!staticArity.empty()) {
            size_t n = 1;
            try {
                n = (size_t)std::stoi(staticArity);
            } catch (...) {
                n = 1;
            }
            es.w("(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(") => " + tn + "." + csIdent(e.name) + "(");
            for (size_t i = 0; i < n; i++) es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
            es.w(")");
            return;
        }
        if (cc == "Math") {
            es.w("Math." + staticMathName(e.name));
            return;
        }
        if ((cc == "int" || cc == "long" || cc == "double" || cc == "float" || cc == "bool") &&
            (e.name == "sum" || e.name == "max" || e.name == "min" || e.name == "compare")) {
            if (e.name == "sum") {
                es.w("((__a, __b) => __a + __b)");
                return;
            }
            if (e.name == "max" || e.name == "min") {
                es.w(std::string("Math.") + (e.name == "max" ? "Max" : "Min"));
                return;
            }
            es.w("((__a, __b) => __a.CompareTo(__b))");
            return;
        }
        if ((cc == "int" && e.name == "parseInt") || (cc == "long" && e.name == "parseLong") ||
            (cc == "double" && e.name == "parseDouble") ||
            (cc == "float" && e.name == "parseFloat") ||
            (cc == "bool" && e.name == "parseBoolean")) {
            es.w(cc + ".Parse");
            return;
        }
        if (cc == "string" || cc == "String") {
            static const std::map<std::string, std::string> sm = {
                {"toUpperCase", "ToUpper()"}, {"toLowerCase", "ToLower()"},
                {"trim", "Trim()"}, {"strip", "Trim()"}};
            auto it = sm.find(e.name);
            if (it != sm.end()) {
                es.w("(__mr => __mr." + it->second + ")");
                return;
            }
            if (e.name == "length") {
                es.w("(__mr => __mr.Length)");
                return;
            }
            if (e.name == "isEmpty") {
                es.w("(__mr => __mr.Length == 0)");
                return;
            }
            if (e.name == "compareTo") {
                es.w("((__m0, __m1) => __m0.CompareTo(__m1))");
                return;
            }
            if (e.name == "format") { es.w("string.Format"); return; }
            if (e.name == "join") { es.w("string.Join"); return; }
            if (e.name == "valueOf") { es.w("Convert.ToString"); return; }
        }
        if (cc == "Objects") {
            if (e.name == "equals") { es.w("object.Equals"); return; }
            if (e.name == "isNull") { es.w("(__mr => __mr == null)"); return; }
            if (e.name == "nonNull") { es.w("(__mr => __mr != null)"); return; }
        }
        if (es.ctx.typeByName.count(tn) && !es.ctx.samDelegates.count(tn)) {
            auto& rets = es.ctx.methodReturns[tn];
            size_t nArgs = 1;
            if (expect.known && startsWith(expect.name, "Func") && expect.args.size() >= 2)
                nArgs = expect.args.size() - 2;
            bool have = rets.count(e.name + "/" + std::to_string(nArgs)) > 0 ||
                        (nArgs == 1 && rets.count(e.name + "/0") > 0);
            if (have) {
                if (nArgs == 0) {
                    es.w("(__mr => __mr." + csIdent(e.name) + "())");
                } else {
                    es.w("(");
                    for (size_t i = 0; i < nArgs; i++)
                        es.w(i ? ", __m" + std::to_string(i) : "__m" + std::to_string(i));
                    es.w(") => __m0." + csIdent(e.name) + "(");
                    for (size_t i = 1; i < nArgs; i++)
                        es.w(i == 1 ? "__m" + std::to_string(i) : ", __m" + std::to_string(i));
                    es.w(")");
                }
                return;
            }
        }
        if (es.ctx.typeByName.count(tn)) {
            es.w(tn + "." + csIdent(e.name));
            return;
        }
    }
    if (e.obj) emitExpr(es, *e.obj, P_POST);
    es.w("." + e.name);
}

void emitSwitchEExpr(ES& es, Expr& e, const TypeInfo& expect) {
    TypeInfo subjT = inferExprType(es, *e.a);
    std::string subjName = subjT.known ? subjT.name : "";
    es.w(captureExprText(es, *e.a, P_PRIMARY));
    es.w(" switch");
    es.nl();
    es.wi();
    es.w("{");
    es.nl();
    es.indent++;
    for (auto& arm : e.cases) {
        es.wi();
        bool first = true;
        if (arm.isDefault) {
            es.w("_");
            first = false;
        }
        if (arm.nullLabel) {
            es.w("null");
            first = false;
        }
        for (auto& pl : arm.patLabels) {
            if (!first) es.w(" or ");
            first = false;
            es.w(csTypeStr(es.ctx, pl, true) + (arm.patName.empty() ? " _" : " " + csIdent(arm.patName)));
        }
        for (auto& l : arm.labels) {
            if (!first) es.w(" or ");
            first = false;
            es.w(enumLabelText(es, *l, subjName));
        }
        if (first) es.w("_");
        if (arm.guard) es.w(" when " + captureExprText(es, *arm.guard, 0));
        es.w(" => ");
        if (arm.value) emitExpr(es, *arm.value, 0, expect);
        else {
            Expr* yieldVal = nullptr;
            for (auto& s : arm.body)
                if (s->k == SK::Return && s->isYield && s->e) yieldVal = s->e.get();
            if (yieldVal) emitExpr(es, *yieldVal, 0, expect);
            else es.w("default");
        }
        es.w(",");
        es.nl();
    }
    es.indent--;
    es.wi();
    es.w("}");
}

void emitCtorCallExpr(ES& es, Expr& e) {
    es.w(e.name == "super" ? "base" : "this");
    es.w("(");
    emitArgs(es, e.args);
    es.w(")");
}

void emitClassLitExpr(ES& es, Expr& e) {
    es.w("typeof(" + csTypeStr(es.ctx, e.ty, true) + ")");
}

void emitCall(ES& es, Expr& e, const TypeInfo& expect) {
    std::string path = e.obj ? pathText(*e.obj) : "";
    if (!e.obj) {
        if (es.ctx.staticImports.count(e.name)) {
            es.w(es.ctx.staticImports[e.name] + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (es.ctx.needStaticMath) {
            std::string mn = staticMathName(e.name);
            if (e.name == "round") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "float") {
                    es.w("(int)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                } else {
                    es.w("(long)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                }
                return;
            }
            if (e.name == "random") {
                es.w("Random.Shared.NextDouble()");
                return;
            }
            if (e.name == "toRadians") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * Math.PI / 180.0)");
                return;
            }
            if (e.name == "toDegrees") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * 180.0 / Math.PI)");
                return;
            }
            if (mn != e.name) {
                es.w("Math." + mn + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        es.w(e.name + "(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    if (path == "System.out") {
        if (e.name == "println") {
            if (e.args.size() == 1) {
                TypeInfo at = inferExprType(es, *e.args[0]);
                if (at.known && at.name == "bool") {
                    es.w("Console.WriteLine((");
                    emitExpr(es, *e.args[0], 0);
                    es.w(") ? \"true\" : \"false\")");
                    return;
                }
            }
            es.w("Console.WriteLine(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "print") {
            if (e.args.size() == 1) {
                TypeInfo at = inferExprType(es, *e.args[0]);
                if (at.known && at.name == "bool") {
                    es.w("Console.Write((");
                    emitExpr(es, *e.args[0], 0);
                    es.w(") ? \"true\" : \"false\")");
                    return;
                }
            }
            es.w("Console.Write(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "printf") {
            FmtXlate fx;
            if (!e.args.empty() && isStringLit(*e.args[0])) fx = translatePrintfFmt(e.args[0]->val);
            es.w("Console.Write(string.Format(\"" + escapeCsStr(fx.fmt) + "\"");
            for (size_t i = 1; i < e.args.size(); i++) {
                es.w(", ");
                emitFmtArg(es, *e.args[i], i - 1 < fx.tags.size() ? fx.tags[i - 1] : 0);
            }
            es.w("))");
            return;
        }
        if (e.name == "flush") {
            es.w("Console.Out.Flush()");
            return;
        }
        es.w("Console.WriteLine(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    if (path == "System.err") {
        if (e.name == "println") {
            es.w("Console.Error.WriteLine(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "print") {
            es.w("Console.Error.Write(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        es.w("Console.Error.WriteLine(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    std::string objClass = e.obj ? lastSeg(pathText(*e.obj)) : "";
    if (objClass == "System" && e.obj && e.obj->k == EK::Ident) {
        if (e.name == "exit") {
            es.w("Environment.Exit(");
            if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
        if (e.name == "currentTimeMillis") {
            es.w("DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()");
            return;
        }
        if (e.name == "nanoTime") {
            es.ctx.needDiag = true;
            es.w("(long)(Stopwatch.GetTimestamp() * 1e9 / Stopwatch.Frequency)");
            return;
        }
        if (e.name == "getProperty") {
            es.w("Environment.GetEnvironmentVariable(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "lineSeparator") {
            es.w("Environment.NewLine");
            return;
        }
        if (e.name == "arraycopy") {
            es.w("Array.Copy(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "getenv") {
            es.w("Environment.GetEnvironmentVariables()");
            return;
        }
    }
    if (e.obj && (e.obj->k == EK::Ident || e.obj->k == EK::Field)) {
        std::string clsName = objClass;
        std::string mappedCls = mapJavaTypeFqn(clsName, true);
        std::string canon = mappedCls.empty() ? clsName : mappedCls;
        if (clsName == "Stream" || clsName == "IntStream" || clsName == "LongStream" ||
            clsName == "DoubleStream") {
            if (e.name == "of") {
                std::string elementType;
                if (!e.targs.empty()) elementType = csTypeStr(es.ctx, e.targs.front(), true);
                else if (expect.known && !expect.args.empty()) elementType = expect.args[0].text();
                else if (clsName == "IntStream") elementType = "int";
                else if (clsName == "LongStream") elementType = "long";
                else if (clsName == "DoubleStream") elementType = "double";
                else if (!e.args.empty()) {
                    TypeInfo element = inferExprType(es, *e.args.front());
                    if (element.known) elementType = element.text();
                }
                if (elementType.empty()) elementType = "object";
                es.ctx.needLinq = true;
                if (e.args.empty()) {
                    es.w("Enumerable.Empty<" + elementType + ">()");
                } else {
                    es.w("new " + elementType + "[] { ");
                    emitArgs(es, e.args);
                    es.w(" }.AsEnumerable()");
                }
                return;
            }
            if (e.name == "empty") {
                es.ctx.needLinq = true;
                std::string elementType;
                if (!e.targs.empty()) elementType = csTypeStr(es.ctx, e.targs.front(), true);
                else if (expect.known && !expect.args.empty()) elementType = expect.args[0].text();
                else if (clsName == "IntStream") elementType = "int";
                else if (clsName == "LongStream") elementType = "long";
                else if (clsName == "DoubleStream") elementType = "double";
                else elementType = "object";
                es.w("Enumerable.Empty<" + elementType + ">()");
                return;
            }
            if (e.name == "range" || e.name == "rangeClosed") {
                es.ctx.needLinq = true;
                if (clsName == "IntStream" || clsName == "LongStream") {
                    es.ctx.needCollectionsSpec = true;
                    es.w(std::string("__JHelp.") +
                         (clsName == "LongStream" ? "LongRange(" : "IntRange("));
                    if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                    else es.w("0");
                    es.w(", ");
                    if (e.args.size() > 1) emitExpr(es, *e.args[1], 0);
                    else es.w("0");
                    es.w(e.name == "rangeClosed" ? ", true)" : ", false)");
                } else {
                    es.w("Enumerable.Range(");
                    if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                    es.w(", ");
                    if (e.args.size() > 1) {
                        if (e.name == "rangeClosed") es.w("(");
                        emitExpr(es, *e.args[1], 0);
                        es.w(" - ");
                        emitExpr(es, *e.args[0], 0);
                        if (e.name == "rangeClosed") es.w(" + 1)");
                    } else es.w("0");
                    es.w(")");
                }
                return;
            }
        }
        if (canon == "Math" || clsName == "StrictMath") {
            std::string n = staticMathName(e.name);
            if (e.name == "round") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "float") {
                    es.w("(int)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                } else {
                    es.w("(long)Math.Floor(");
                    emitArgs(es, e.args);
                    es.w(" + 0.5)");
                }
                return;
            }
            if (e.name == "random") {
                es.w("Random.Shared.NextDouble()");
                return;
            }
            if (e.name == "toRadians") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * Math.PI / 180.0)");
                return;
            }
            if (e.name == "toDegrees") {
                es.w("(");
                emitArgs(es, e.args);
                es.w(" * 180.0 / Math.PI)");
                return;
            }
            if (e.name == "floorDiv" || e.name == "floorMod") {
                es.w("Math." + n + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "abs" || e.name == "max" || e.name == "min") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "decimal") {
                    es.w("Math." + n + "(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (at.known && at.name == "long") {
                    es.w("Math." + n + "(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
            }
            es.w("Math." + n + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (canon == "int" || canon == "Integer" || canon == "long" || canon == "Long" ||
            canon == "short" || canon == "Short" || canon == "byte" || canon == "Byte") {
            std::string prim = canon == "int" || canon == "Integer" ? "int"
                               : canon == "long" || canon == "Long"  ? "long"
                               : canon == "short" || canon == "Short" ? "short"
                                                       : "byte";
            if (e.name == "parseInt" || e.name == "parseLong" || e.name == "parseShort" ||
                e.name == "parseByte") {
                es.w(prim + ".Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                if (e.args.size() == 1) {
                    TypeInfo at = inferExprType(es, *e.args[0]);
                    if (at.known && at.name == "string") {
                        es.w(prim + ".Parse(");
                        emitExpr(es, *e.args[0], 0);
                        es.w(")");
                    } else {
                        emitExpr(es, *e.args[0], 0);
                    }
                    return;
                }
            }
            if (e.name == "toString") {
                if (!e.args.empty()) {
                    TypeInfo at = inferExprType(es, *e.args[0]);
                    if (at.known && isNumericCs(at.name)) {
                        es.w("(");
                        emitExpr(es, *e.args[0], P_POST);
                        es.w(").ToString()");
                    } else {
                        es.w("Convert.ToString(");
                        emitArgs(es, e.args);
                        es.w(")");
                    }
                    return;
                }
            }
            if (e.name == "max" || e.name == "min") {
                es.w(std::string("Math.") + (e.name == "max" ? "Max" : "Min") + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "compare") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").CompareTo(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "sum") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" + ");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "toBinaryString") {
                es.w("Convert.ToString(");
                emitExpr(es, *e.args[0], 0);
                es.w(", 2)");
                return;
            }
            if (e.name == "toHexString") {
                es.w("Convert.ToString(");
                emitExpr(es, *e.args[0], 0);
                es.w(", 16)");
                return;
            }
            if (e.name == "toOctalString") {
                es.w("Convert.ToString(");
                emitExpr(es, *e.args[0], 0);
                es.w(", 8)");
                return;
            }
            if (e.name == "bitCount") {
                es.ctx.needNumerics = true;
                es.w("System.Numerics.BitOperations.PopCount((uint)(");
                emitExpr(es, *e.args[0], 0);
                es.w("))");
                return;
            }
            if (e.name == "hashCode") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").GetHashCode()");
                return;
            }
        }
        if (canon == "double" || canon == "Double" || canon == "float" || canon == "Float") {
            std::string prim = canon;
            if (e.name == "parseDouble" || e.name == "parseFloat") {
                es.w(prim + ".Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "string") {
                    es.w(prim + ".Parse(");
                    emitExpr(es, *e.args[0], 0);
                    es.w(")");
                } else if (!e.args.empty()) {
                    emitExpr(es, *e.args[0], 0);
                }
                return;
            }
            if (e.name == "isNaN") {
                es.w(prim + ".IsNaN(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "isInfinite") {
                es.w(prim + ".IsInfinity(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "isFinite") {
                es.w(prim + ".IsFinite(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "compare") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").CompareTo(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "toString") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").ToString()");
                return;
            }
        }
        if (canon == "bool" || canon == "Boolean") {
            if (e.name == "parseBoolean") {
                es.w("bool.Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                TypeInfo at = e.args.empty() ? TypeInfo() : inferExprType(es, *e.args[0]);
                if (at.known && at.name == "string") {
                    es.w("bool.Parse(");
                    emitExpr(es, *e.args[0], 0);
                    es.w(")");
                } else if (!e.args.empty()) {
                    emitExpr(es, *e.args[0], 0);
                }
                return;
            }
            if (e.name == "toString") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").ToString()");
                return;
            }
            if (e.name == "compare") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").CompareTo(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
        }
        if (canon == "char" || canon == "Character") {
            static const std::map<std::string, std::string> charM = {
                {"isDigit", "IsDigit"}, {"isLetter", "IsLetter"},
                {"isLetterOrDigit", "IsLetterOrDigit"}, {"isWhitespace", "IsWhiteSpace"},
                {"isUpperCase", "IsUpper"}, {"isLowerCase", "IsLower"},
                {"toUpperCase", "ToUpper"}, {"toLowerCase", "ToLower"},
                {"isAlphabetic", "IsLetter"}, {"isSpaceChar", "IsWhiteSpace"}};
            auto cm = charM.find(e.name);
            if (cm != charM.end()) {
                es.w("char." + cm->second + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "getNumericValue") {
                es.w("((int)char.GetNumericValue(");
                emitArgs(es, e.args);
                es.w("))");
                return;
            }
            if (e.name == "toString") {
                es.w("(");
                emitExpr(es, *e.args[0], P_POST);
                es.w(").ToString()");
                return;
            }
        }
        if (canon == "string" || canon == "String") {
            if (e.name == "format") {
                FmtXlate fx;
                if (!e.args.empty() && isStringLit(*e.args[0])) fx = translatePrintfFmt(e.args[0]->val);
                es.w("string.Format(\"" + escapeCsStr(fx.fmt) + "\"");
                for (size_t i = 1; i < e.args.size(); i++) {
                    es.w(", ");
                    emitFmtArg(es, *e.args[i], i - 1 < fx.tags.size() ? fx.tags[i - 1] : 0);
                }
                es.w(")");
                return;
            }
            if (e.name == "valueOf") {
                if (!e.args.empty()) {
                    TypeInfo at = inferExprType(es, *e.args[0]);
                    if (at.known && at.dims > 0 && at.name == "char") {
                        es.w("new string(");
                        emitExpr(es, *e.args[0], 0);
                        es.w(")");
                    } else {
                        es.w("Convert.ToString(");
                        emitExpr(es, *e.args[0], 0);
                        es.w(")");
                    }
                    return;
                }
            }
            if (e.name == "join") {
                es.w("string.Join(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "isEmpty") {
                es.w("string.IsNullOrEmpty(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "Objects") {
            if (e.name == "equals") {
                es.w("object.Equals(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "requireNonNull") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" ?? throw new ArgumentNullException())");
                return;
            }
            if (e.name == "requireNonNullElse") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" ?? ");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "isNull") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" == null)");
                return;
            }
            if (e.name == "nonNull") {
                es.w("(");
                emitExpr(es, *e.args[0], 0);
                es.w(" != null)");
                return;
            }
            if (e.name == "hash") {
                es.w("System.HashCode.Combine(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "toString") {
                es.w("Convert.ToString(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "List" || clsName == "List" || clsName == "ArrayList") {
            if (e.name == "of") {
                TypeInfo et;
                bool haveEt = false;
                if (expect.known && !expect.args.empty()) {
                    et = expect.args[0];
                    haveEt = true;
                } else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known && a0.name != "null") {
                        et = a0;
                        haveEt = true;
                    }
                }
                es.w("new List<" + (haveEt ? et.text() : "object") + "> { ");
                emitArgs(es, e.args);
                es.w(" }");
                return;
            }
        }
        if (clsName == "Set" || clsName == "HashSet") {
            if (e.name == "of") {
                TypeInfo et;
                bool haveEt = false;
                if (expect.known && !expect.args.empty()) {
                    et = expect.args[0];
                    haveEt = true;
                } else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known && a0.name != "null") {
                        et = a0;
                        haveEt = true;
                    }
                }
                es.w("new HashSet<" + (haveEt ? et.text() : "object") + "> { ");
                emitArgs(es, e.args);
                es.w(" }");
                return;
            }
        }
        if (clsName == "Map" || clsName == "HashMap") {
            if (e.name == "of" && e.args.size() % 2 == 0) {
                TypeInfo keyType;
                TypeInfo valueType;
                if (expect.known && expect.args.size() >= 2) {
                    keyType = expect.args[0];
                    valueType = expect.args[1];
                } else if (e.args.size() >= 2) {
                    keyType = inferExprType(es, *e.args[0]);
                    valueType = inferExprType(es, *e.args[1]);
                }
                std::string keyName = keyType.known ? keyType.text() : "object";
                std::string valueName = valueType.known ? valueType.text() : "object";
                es.w("new Dictionary<" + keyName + ", " + valueName + "> { ");
                for (size_t i = 0; i + 1 < e.args.size(); i += 2) {
                    if (i) es.w(", ");
                    es.w("{ ");
                    emitExpr(es, *e.args[i], 0, keyType);
                    es.w(", ");
                    emitExpr(es, *e.args[i + 1], 0, valueType);
                    es.w(" }");
                }
                es.w(" }");
                return;
            }
        }
        if (es.ctx.simpleEnums.count(clsName) || es.ctx.enumClassTypes.count(clsName)) {
            if (e.name == "values") {
                if (es.ctx.simpleEnums.count(clsName))
                    es.w("((" + clsName + "[])Enum.GetValues(typeof(" + clsName + ")))");
                else
                    es.w("(" + clsName + ".Values())");
                return;
            }
            if (e.name == "valueOf") {
                if (es.ctx.simpleEnums.count(clsName)) {
                    es.w("((" + clsName + ")Enum.Parse(typeof(" + clsName + "), ");
                    if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                    es.w("))");
                } else {
                    es.w("(" + clsName + ".ValueOf(");
                    emitArgs(es, e.args);
                    es.w("))");
                }
                return;
            }
        }
        if (clsName == "Files" || clsName == "Paths") {
            es.ctx.needIO = true;
            if (clsName == "Paths" && e.name == "get") {
                if (e.args.size() == 1) {
                    emitExpr(es, *e.args[0], 0);
                } else {
                    es.w("Path.Combine(");
                    emitArgs(es, e.args);
                    es.w(")");
                }
                return;
            }
            if (clsName == "Files" && e.name == "exists") {
                es.w("File.Exists(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "delete") {
                es.w("File.Delete(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "readAllLines") {
                es.w("File.ReadAllLines(");
                emitArgs(es, e.args);
                es.w(").ToList()");
                return;
            }
            if (clsName == "Files" && e.name == "readAllBytes") {
                es.w("File.ReadAllBytes(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "write") {
                es.w("File.WriteAllBytes(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (clsName == "Files" && e.name == "createTempFile") {
                es.w("Path.Combine(Path.GetTempPath(), ");
                if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                else es.w("\"tmp\"");
                es.w(" + \"-\" + Guid.NewGuid().ToString(\"N\") + ");
                if (e.args.size() > 1) emitExpr(es, *e.args[1], 0);
                else es.w("\".tmp\"");
                es.w(")");
                return;
            }
        }
        if (canon == "Comparison" || clsName == "Comparator") {
            if (e.name == "naturalOrder") {
                es.w("((a, b) => Comparer<object>.Default.Compare(a, b))");
                return;
            }
            if (e.name == "reverseOrder") {
                es.w("((a, b) => Comparer<object>.Default.Compare(b, a))");
                return;
            }
        }
        if (canon == "Array" || canon == "Arrays") {
            std::string n = arraysStaticName(e.name);
            if (e.name == "asList") {
                TypeInfo et;
                bool haveEt = false;
                if (expect.known && !expect.args.empty()) {
                    et = expect.args[0];
                    haveEt = true;
                } else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known && a0.dims == 0 && a0.name != "null") {
                        et = a0;
                        haveEt = true;
                    }
                }
                if (haveEt) {
                    es.w("new List<" + et.text() + "> { ");
                    for (size_t i = 0; i < e.args.size(); i++) {
                        if (i) es.w(", ");
                        emitExpr(es, *e.args[i], 0, et);
                    }
                    es.w(" }");
                } else {
                    es.w("new List<object> { ");
                    emitArgs(es, e.args);
                    es.w(" }");
                }
                return;
            }
            if (e.name == "sort") {
                if (e.args.size() == 1) {
                    es.w("Array.Sort(" + captureExprText(es, *e.args[0], P_POST) + ")");
                    return;
                }
                if (e.args.size() == 2) {
                    es.w("Array.Sort(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                    emitExpr(es, *e.args[1], 0);
                    es.w(")");
                    return;
                }
                if (e.args.size() == 3) {
                    es.w("Array.Sort(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                    emitExpr(es, *e.args[1], 0);
                    es.w(", (" + captureExprText(es, *e.args[2], 0) + ") - (" +
                         captureExprText(es, *e.args[1], 0) + "))");
                    return;
                }
            }
            if (e.name == "fill") {
                es.w("Array.Fill(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "copyOf") {
                es.ctx.needCollectionsSpec = true;
                es.w("__JHelp.CopyOf(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "copyOfRange") {
                es.ctx.needCollectionsSpec = true;
                es.w("__JHelp.CopyOfRange(" + captureExprText(es, *e.args[0], P_POST) + ", ");
                emitExpr(es, *e.args[1], 0);
                es.w(", ");
                emitExpr(es, *e.args[2], 0);
                es.w(")");
                return;
            }
            if (e.name == "toString" || e.name == "deepToString") {
                es.ctx.needLinq = true;
                es.w("$\"[{string.Join(\", \", ");
                emitExpr(es, *e.args[0], 0);
                es.w(")}]\"");
                return;
            }
            if (e.name == "equals") {
                es.ctx.needLinq = true;
                es.w(captureExprText(es, *e.args[0], P_POST) + ".SequenceEqual(");
                emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "binarySearch") {
                es.w("Array.BinarySearch(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "hashCode") {
                es.w("Array.Empty<int>().GetHashCode()");
                return;
            }
            es.w("Array." + n + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (canon == "Collections") {
            if (e.name == "sort") {
                if (!e.args.empty()) {
                    es.w(captureExprText(es, *e.args[0], P_POST) + ".Sort(");
                    for (size_t i = 1; i < e.args.size(); i++) {
                        if (i > 1) es.w(", ");
                        emitExpr(es, *e.args[i], 0);
                    }
                    es.w(")");
                    return;
                }
            }
            if (e.name == "reverse") {
                if (!e.args.empty()) {
                    es.ctx.needLinq = true;
                    es.w(captureExprText(es, *e.args[0], P_POST) + " = " +
                         captureExprText(es, *e.args[0], P_POST) + ".AsEnumerable().Reverse().ToList()");
                    return;
                }
            }
            if (e.name == "max" || e.name == "min") {
                es.ctx.needLinq = true;
                es.w(captureExprText(es, *e.args[0], P_POST) + "." +
                     (e.name == "max" ? "Max" : "Min") + "()");
                return;
            }
            if (e.name == "unmodifiableList" || e.name == "unmodifiableCollection") {
                if (!e.args.empty()) {
                    es.w(captureExprText(es, *e.args[0], P_POST) + ".AsReadOnly()");
                    return;
                }
            }
            if (e.name == "unmodifiableMap" || e.name == "unmodifiableSet" ||
                e.name == "singleton" || e.name == "unmodifiableSortedMap") {
                if (!e.args.empty()) {
                    es.w(captureExprText(es, *e.args[0], P_POST));
                    return;
                }
            }
            if (e.name == "emptyList") {
                if (expect.known && expect.name == "List" && !expect.args.empty())
                    es.w("new List<" + expect.args[0].text() + ">()");
                else es.w("new List<object>()");
                return;
            }
            if (e.name == "emptyMap") {
                if (expect.known && expect.name == "Dictionary" && expect.args.size() == 2)
                    es.w("new Dictionary<" + expect.args[0].text() + ", " +
                         expect.args[1].text() + ">()");
                else es.w("new Dictionary<object, object>()");
                return;
            }
            if (e.name == "emptySet") {
                if (expect.known && expect.name == "HashSet" && !expect.args.empty())
                    es.w("new HashSet<" + expect.args[0].text() + ">()");
                else es.w("new HashSet<object>()");
                return;
            }
            if (e.name == "singletonList") {
                TypeInfo et;
                if (expect.known && expect.name == "List" && !expect.args.empty()) et = expect.args[0];
                else if (!e.args.empty()) {
                    TypeInfo a0 = inferExprType(es, *e.args[0]);
                    if (a0.known) et = a0;
                }
                es.w("new List<" + (et.known ? et.text() : "object") + "> { ");
                if (!e.args.empty()) emitExpr(es, *e.args[0], 0, et);
                es.w(" }");
                return;
            }
            if (e.name == "nCopies") {
                es.ctx.needLinq = true;
                es.w("Enumerable.Repeat(");
                emitExpr(es, *e.args[1], 0);
                es.w(", ");
                emitExpr(es, *e.args[0], 0);
                es.w(").ToList()");
                return;
            }
            if (e.name == "binarySearch") {
                es.w(captureExprText(es, *e.args[0], P_POST) + ".BinarySearch(");
                if (e.args.size() > 1) emitExpr(es, *e.args[1], 0);
                es.w(")");
                return;
            }
            if (e.name == "swap") {
                if (e.args.size() == 3) {
                    std::string l = captureExprText(es, *e.args[0], P_POST);
                    es.w("((" + l + "[" + captureExprText(es, *e.args[1], 0) + "], " + l + "[" +
                         captureExprText(es, *e.args[2], 0) + "]) = (" + l + "[" +
                         captureExprText(es, *e.args[2], 0) + "], " + l + "[" +
                         captureExprText(es, *e.args[1], 0) + "]))");
                    return;
                }
            }
        }
        if (canon == "Guid" || canon == "UUID") {
            if (e.name == "randomUUID") {
                es.w("Guid.NewGuid()");
                return;
            }
            if (e.name == "fromString") {
                es.w("Guid.Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "Thread") {
            if (e.name == "sleep") {
                es.ctx.needThreading = true;
                es.w("Thread.Sleep(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "currentThread") {
                es.w("Thread.CurrentThread");
                return;
            }
        }
        if (clsName == "LocalDate" || clsName == "LocalDateTime" || clsName == "Instant" ||
            clsName == "LocalTime" || clsName == "Duration" || clsName == "ZonedDateTime" ||
            clsName == "Period") {
            if (e.name == "now") {
                if (clsName == "LocalDate") es.w("DateOnly.FromDateTime(DateTime.Now)");
                else if (clsName == "LocalTime") es.w("TimeOnly.FromDateTime(DateTime.Now)");
                else if (clsName == "LocalDateTime") es.w("DateTime.Now");
                else if (clsName == "Instant" || clsName == "ZonedDateTime")
                    es.w("DateTimeOffset.UtcNow");
                else es.w("TimeSpan.Zero");
                return;
            }
            if (e.name == "of") {
                if (clsName == "LocalDate") {
                    es.w("new DateOnly(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (clsName == "LocalTime") {
                    es.w("new TimeOnly(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (clsName == "LocalDateTime") {
                    es.w("new DateTime(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
                if (clsName == "Duration") {
                    static const std::map<std::string, std::string> durM = {
                        {"ofMillis", "FromMilliseconds"}, {"ofSeconds", "FromSeconds"},
                        {"ofMinutes", "FromMinutes"}, {"ofHours", "FromHours"},
                        {"ofDays", "FromDays"}};
                    if (!e.args.empty() && isStringLit(*e.args[0]) == false) {
                    }
                    es.w("TimeSpan.FromMilliseconds(");
                    emitArgs(es, e.args);
                    es.w(")");
                    return;
                }
            }
            if (e.name == "parse") {
                if (clsName == "LocalDate") es.w("DateOnly.Parse(");
                else if (clsName == "LocalTime") es.w("TimeOnly.Parse(");
                else if (clsName == "Instant") es.w("DateTimeOffset.Parse(");
                else es.w("DateTime.Parse(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "between") {
                es.w("(");
                emitExpr(es, *e.args[1], 0);
                es.w(" - ");
                emitExpr(es, *e.args[0], 0);
                es.w(")");
                return;
            }
            if (e.name == "ofMillis" || e.name == "ofSeconds" || e.name == "ofMinutes" ||
                e.name == "ofHours" || e.name == "ofDays") {
                static const std::map<std::string, std::string> durM2 = {
                    {"ofMillis", "FromMilliseconds"}, {"ofSeconds", "FromSeconds"},
                    {"ofMinutes", "FromMinutes"}, {"ofHours", "FromHours"},
                    {"ofDays", "FromDays"}};
                es.w("TimeSpan." + durM2.at(e.name) + "(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
        }
        if (canon == "Regex" || clsName == "Pattern") {
            if (e.name == "compile") {
                es.w("new Regex(");
                emitArgs(es, e.args);
                es.w(")");
                return;
            }
            if (e.name == "matches") {
                es.ctx.needRegex = true;
                es.w("Regex.IsMatch(");
                if (e.args.size() >= 2) {
                    emitExpr(es, *e.args[1], 0);
                    es.w(", ");
                    emitExpr(es, *e.args[0], 0);
                }
                es.w(")");
                return;
            }
        }
        if (clsName == "Optional") {
            if (e.name == "ofNullable" || e.name == "of") {
                if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
                return;
            }
            if (e.name == "empty") {
                es.w("null");
                return;
            }
        }
        if (canon == "decimal" || clsName == "BigDecimal") {
            if (e.name == "valueOf") {
                if (!e.args.empty()) {
                    es.w(std::string("((") + (inferExprType(es, *e.args[0]).name == "long" ? "long" : "decimal") +
                         ")(");
                    emitExpr(es, *e.args[0], 0);
                    es.w("))");
                    return;
                }
            }
        }
        if (e.obj->k == EK::Ident && es.ctx.typeByName.count(clsName)) {
            Decl* cd = es.ctx.typeByName[clsName];
            auto& rets = es.ctx.methodReturns[clsName];
            auto it = rets.find(e.name + "/" + std::to_string(e.args.size()));
            (void)cd;
            (void)it;
        }
    }
    if (e.obj->k == EK::Call && e.name == "matches") {
        Expr* inner = e.obj.get();
        if (inner->name == "matcher" && inner->obj && inner->obj->k == EK::Call &&
            inner->obj->name == "compile" && inner->obj->obj &&
            lastSeg(pathText(*inner->obj->obj)) == "Pattern") {
            es.ctx.needRegex = true;
            es.w("Regex.IsMatch(");
            emitExpr(es, *inner->args[0], 0);
            es.w(", ");
            emitExpr(es, *inner->obj->args[0], 0);
            es.w(")");
            return;
        }
    }
    if (e.obj->k == EK::Call && e.name == "replaceAll") {
        Expr* inner = e.obj.get();
        if (inner->name == "matcher" && inner->obj && inner->obj->k == EK::Call &&
            inner->obj->name == "compile" && inner->obj->obj &&
            lastSeg(pathText(*inner->obj->obj)) == "Pattern") {
            es.ctx.needRegex = true;
            es.w("Regex.Replace(");
            emitExpr(es, *inner->args[0], 0);
            es.w(", ");
            emitExpr(es, *inner->obj->args[0], 0);
            es.w(", ");
            emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
    }
    TypeInfo rt;
    if (e.obj) rt = inferExprType(es, *e.obj);
    std::string recv = e.obj ? captureExprText(es, *e.obj, P_POST) : "";
    std::string rname = rt.known ? rt.name : "";
    if (rt.known && e.args.empty()) {
        auto rcit = es.ctx.recordComps.find(rname);
        if (rcit != es.ctx.recordComps.end()) {
            bool isComp = false;
            for (auto& cn : rcit->second)
                if (cn == e.name) isComp = true;
            if (isComp) {
                es.w(recv + "." + pascalCase(e.name));
                return;
            }
        }
    }
    if (e.name == "stream" && rt.known) {
        es.w(recv);
        return;
    }
    if (e.name == "boxed" && rt.known) {
        es.w(recv);
        return;
    }
    if (e.name == "entrySet" && rname == "Dictionary") {
        es.w(recv);
        return;
    }
        if (rname == "string") {
        if (e.name == "length") { es.w(recv + ".Length"); return; }
        if (e.name == "charAt") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "substring") {
            if (e.args.size() == 1) {
                es.w(recv + ".Substring(" + captureExprText(es, *e.args[0], 0) + ")");
            } else {
                es.w(recv + ".Substring(" + captureExprText(es, *e.args[0], 0) + ", " +
                     captureExprText(es, *e.args[1], 0) + " - " +
                     captureExprText(es, *e.args[0], 0) + ")");
            }
            return;
        }
        if (e.name == "indexOf") { es.w(recv + ".IndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "lastIndexOf") { es.w(recv + ".LastIndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "equals") {
            es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_ADD) + ")");
            return;
        }
        if (e.name == "equalsIgnoreCase") {
            es.w("string.Equals(" + recv + ", " + captureExprText(es, *e.args[0], 0) +
                 ", StringComparison.OrdinalIgnoreCase)");
            return;
        }
        if (e.name == "contains") { es.w(recv + ".Contains("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "toUpperCase") { es.w(recv + ".ToUpper()"); return; }
        if (e.name == "toLowerCase") { es.w(recv + ".ToLower()"); return; }
        if (e.name == "trim" || e.name == "strip") { es.w(recv + ".Trim()"); return; }
        if (e.name == "replace") { es.w(recv + ".Replace("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "replaceAll") {
            es.ctx.needRegex = true;
            es.w("Regex.Replace(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(", ");
            emitExpr(es, *e.args[1], 0);
            es.w(")");
            return;
        }
        if (e.name == "replaceFirst") {
            es.ctx.needRegex = true;
            es.w("Regex.Replace(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(", ");
            emitExpr(es, *e.args[1], 0);
            es.w(", 1)");
            return;
        }
        if (e.name == "matches") {
            es.ctx.needRegex = true;
            es.w("Regex.IsMatch(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
        if (e.name == "startsWith") { es.w(recv + ".StartsWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "endsWith") { es.w(recv + ".EndsWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Length == 0)"); return; }
        if (e.name == "isBlank") { es.w("string.IsNullOrWhiteSpace(" + recv + ")"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "compareToIgnoreCase") {
            es.w("string.Compare(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ", true)");
            return;
        }
        if (e.name == "split") {
            es.ctx.needRegex = true;
            es.w("Regex.Split(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w(")");
            return;
        }
        if (e.name == "toCharArray") { es.w(recv + ".ToCharArray()"); return; }
        if (e.name == "hashCode") { es.w(recv + ".GetHashCode()"); return; }
        if (e.name == "getClass") { es.w(recv + ".GetType()"); return; }
        if (e.name == "toString") { es.w(recv); return; }
        if (e.name == "concat") {
            es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")");
            return;
        }
        if (e.name == "repeat") {
            es.ctx.needLinq = true;
            es.w("string.Concat(Enumerable.Repeat(" + recv + ", ");
            emitExpr(es, *e.args[0], 0);
            es.w("))");
            return;
        }
        if (e.name == "intern") { es.w(std::string("string.Intern(") + recv + ")"); return; }
        if (e.name == "chars") { es.w(recv); return; }
        if (e.name == "bytes" || e.name == "getBytes") { es.ctx.needText = true; es.w("Encoding.UTF8.GetBytes(" + recv + ")"); return; }
        if (e.name == "codePointAt") {
            es.w("(int)" + recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
    }
    if (rt.dims > 0) {
        if (e.name == "length") { es.w(recv + ".Length"); return; }
        if (e.name == "clone") {
            es.w("(" + csTypeStr(es.ctx, e.ty) + ")" + recv + ".Clone()");
            return;
        }
    }
    if (rname == "List" || rname == "IList" || rname == "LinkedList" || rname == "ICollection") {
        if (e.name == "size") { es.w(recv + ".Count"); return; }
        if (e.name == "get" && rname != "ICollection") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "set" && rname != "ICollection") {
            es.w("(" + recv + "[" + captureExprText(es, *e.args[0], 0) + "] = " +
                 captureExprText(es, *e.args[1], 0) + ")");
            return;
        }
        if (e.name == "add") {
            if (e.args.size() == 2 && rname != "ICollection") {
                es.w(recv + ".Insert(" + captureExprText(es, *e.args[0], 0) + ", " +
                     captureExprText(es, *e.args[1], 0) + ")");
            } else if (rname == "LinkedList") {
                es.w(recv + ".AddLast(");
                emitArgs(es, e.args);
                es.w(")");
            } else {
                es.w(recv + ".Add(");
                emitArgs(es, e.args);
                es.w(")");
            }
            return;
        }
        if (e.name == "remove") {
            if (e.args.size() == 1) {
                TypeInfo at = inferExprType(es, *e.args[0]);
                bool idx = at.known && (at.name == "int" || at.name == "long" || at.name == "short" ||
                                        at.name == "byte" || at.name == "char");
                if (!at.known && e.args[0]->k == EK::Lit && e.args[0]->lt == TT::Int) idx = true;
                if (rname != "ICollection") {
                    if (idx) es.w(recv + ".RemoveAt(" + captureExprText(es, *e.args[0], 0) + ")");
                    else es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ")");
                } else es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ")");
                return;
            }
        }
        if (e.name == "contains") { es.w(recv + ".Contains("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "indexOf") { es.w(recv + ".IndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "lastIndexOf") { es.w(recv + ".LastIndexOf("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "clear") { es.w(recv + ".Clear()"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "addAll" && rname != "ICollection") { es.w(recv + ".AddRange("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "removeAll" && rname != "ICollection") {
            es.w(recv + ".RemoveAll(x => " + captureExprText(es, *e.args[0], P_PRIMARY) +
                 ".Contains(x))");
            es.ctx.needLinq = true;
            return;
        }
        if (e.name == "removeIf" && rname != "ICollection") { es.w(recv + ".RemoveAll("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "sort") { es.w(recv + ".Sort("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "forEach" && rname != "ICollection") { es.w(recv + ".ForEach("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "iterator") { es.w(recv + ".GetEnumerator()"); return; }
        if (e.name == "toArray" && rname != "ICollection") { es.w(recv + ".ToArray()"); return; }
        if (e.name == "subList" && rname == "List") {
            es.w(recv + ".GetRange(" + captureExprText(es, *e.args[0], 0) + ", " +
                 captureExprText(es, *e.args[1], 0) + " - " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "getFirst") {
            es.ctx.needLinq = true;
            es.w(rname == "LinkedList" ? recv + ".First.Value" : recv + ".First()");
            return;
        }
        if (e.name == "peek" || e.name == "element") {
            es.ctx.needLinq = true;
            es.w(rname == "LinkedList" ? recv + ".First.Value" : recv + ".First()");
            return;
        }
        if (e.name == "getLast" || e.name == "peekLast") {
            es.ctx.needLinq = true;
            es.w(rname == "LinkedList" ? recv + ".Last.Value" : recv + ".Last()");
            return;
        }
        if (e.name == "poll" || e.name == "pollFirst" ||
            ((e.name == "remove") && e.args.empty())) {
            es.ctx.needLinq = true;
            es.ctx.needCollectionsSpec = true;
            if (rname == "LinkedList") {
                es.w("__JHelp.Poll(" + recv + ")");
            } else if (rname == "List" || rname == "IList") {
                es.w("__JHelp.Poll(" + recv + ")");
            } else es.w(recv + ".FirstOrDefault()");
            return;
        }
        if (e.name == "pollLast") {
            es.ctx.needCollectionsSpec = true;
            es.w("__JHelp.PollLast(" + recv + ")");
            return;
        }
        if (e.name == "addFirst") {
            if (rname == "LinkedList") es.w(recv + ".AddFirst(");
            else es.w(recv + ".Insert(0, ");
            emitArgs(es, e.args);
            es.w(rname == "LinkedList" ? ")" : ")");
            return;
        }
        if (e.name == "addLast") {
            if (rname == "LinkedList") es.w(recv + ".AddLast(");
            else es.w(recv + ".Add(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "removeFirst") {
            if (rname == "LinkedList") es.w(recv + ".RemoveFirst()");
            else es.w(recv + ".RemoveAt(0)");
            return;
        }
        if (e.name == "removeLast") {
            if (rname == "LinkedList") es.w(recv + ".RemoveLast()");
            else es.w(recv + ".RemoveAt(" + recv + ".Count - 1)");
            return;
        }
        if (e.name == "push") {
            if (rname == "LinkedList") es.w(recv + ".AddFirst(");
            else es.w(recv + ".Insert(0, ");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "pop") {
            es.ctx.needCollectionsSpec = true;
            if (rname == "LinkedList") es.w("__JHelp.PollLast(" + recv + ")");
            else es.w("__JHelp.Poll(" + recv + ")");
            return;
        }
        if (e.name == "offer") {
            if (rname == "LinkedList") es.w(recv + ".AddLast(");
            else es.w(recv + ".Add(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
    }
    if (rname == "HashSet" || rname == "SortedSet" || rname == "ISet") {
        if (e.name == "add") { es.w(recv + ".Add("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "contains") { es.w(recv + ".Contains("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "remove") { es.w(recv + ".Remove("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "size") { es.w(recv + ".Count"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "addAll") { es.w(recv + ".UnionWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "removeAll") { es.w(recv + ".ExceptWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "retainAll") { es.w(recv + ".IntersectWith("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "clear") { es.w(recv + ".Clear()"); return; }
        if (e.name == "iterator") { es.w(recv + ".GetEnumerator()"); return; }
        if (e.name == "toArray") { es.w(recv + ".ToArray()"); return; }
    }
    if (rname == "Dictionary" || rname == "SortedDictionary") {
        if (e.name == "put") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], P_PRIMARY) + "] = " +
                 captureExprText(es, *e.args[1], 0));
            return;
        }
        if (e.name == "get") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "getOrDefault") {
            std::string tmp = "__g" + std::to_string(es.tmpCounter++);
            es.w("(" + recv + ".TryGetValue(" + captureExprText(es, *e.args[0], 0) + ", out var " +
                 tmp + ") ? " + tmp + " : " + captureExprText(es, *e.args[1], 0) + ")");
            return;
        }
        if (e.name == "containsKey") { es.w(recv + ".ContainsKey("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "containsValue") { es.w(recv + ".ContainsValue("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "remove") { es.w(recv + ".Remove("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "size") { es.w(recv + ".Count"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "keySet") { es.w(recv + ".Keys"); return; }
        if (e.name == "values") { es.w(recv + ".Values"); return; }
        if (e.name == "clear") { es.w(recv + ".Clear()"); return; }
    }
    if (rname == "StringBuilder") {
        if (e.name == "append") { es.w(recv + ".Append("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "insert") { es.w(recv + ".Insert("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "length") { es.w(recv + ".Length"); return; }
        if (e.name == "charAt") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "]");
            return;
        }
        if (e.name == "setCharAt") {
            es.w(recv + "[" + captureExprText(es, *e.args[0], 0) + "] = " +
                 captureExprText(es, *e.args[1], 0));
            return;
        }
        if (e.name == "deleteCharAt") {
            es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ", 1)");
            return;
        }
        if (e.name == "delete") {
            es.w(recv + ".Remove(" + captureExprText(es, *e.args[0], 0) + ", " +
                 captureExprText(es, *e.args[1], 0) + " - " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "setLength") {
            es.w("(" + recv + ".Length = " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "reverse") {
            es.ctx.needLinq = true;
            es.w("new string(" + recv + ".ToString().Reverse().ToArray())");
            return;
        }
        if (e.name == "indexOf") {
            es.w(recv + ".ToString().IndexOf(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "isEmpty") { es.w("(" + recv + ".Length == 0)"); return; }
    }
    if (rname == "Stack") {
        if (e.name == "push") { es.w(recv + ".Push("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "pop") { es.w(recv + ".Pop()"); return; }
        if (e.name == "peek") { es.w(recv + ".Peek()"); return; }
        if (e.name == "empty") { es.w("(" + recv + ".Count == 0)"); return; }
        if (e.name == "size") { es.w(recv + ".Count"); return; }
    }
    if (es.ctx.simpleEnums.count(rname)) {
        if (e.name == "compareTo" && e.args.size() == 1) {
            es.w("(((int)" + recv + ").CompareTo((int)" + captureExprText(es, *e.args[0], P_PRIMARY) + "))");
            return;
        }
        if (e.name == "equals" && e.args.size() == 1) {
            es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_EQ) + ")");
            return;
        }
        if (e.name == "toString" && e.args.empty()) { es.w(recv + ".ToString()"); return; }
        if (e.name == "ordinal" && e.args.empty()) { es.w("(int)" + recv); return; }
        if (e.name == "name" && e.args.empty()) { es.w(recv + ".ToString()"); return; }
    }
    if (es.ctx.enumClassTypes.count(rname)) {
        if (e.name == "compareTo" && e.args.size() == 1) {
            es.w("(" + recv + ".Ordinal - " + captureExprText(es, *e.args[0], P_ADD) + ".Ordinal)");
            return;
        }
        if (e.name == "equals" && e.args.size() == 1) {
            es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_EQ) + ")");
            return;
        }
        if (e.name == "ordinal" && e.args.empty()) { es.w(recv + ".Ordinal"); return; }
        if (e.name == "name" && e.args.empty()) { es.w(recv + ".Name"); return; }
    }
    if (rname == "IEnumerator") {
        if (e.name == "hasNext") { es.w(recv + ".MoveNext()"); return; }
        if (e.name == "next") { es.w(recv + ".Current"); return; }
    }
    if (endsWith(rname, "?")) {
        if (e.name == "isPresent") { es.w("(" + recv + " != null)"); return; }
        if (e.name == "isEmpty") { es.w("(" + recv + " == null)"); return; }
        if (e.name == "orElse") {
            es.w("(" + recv + " ?? " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "orElseGet") {
            es.w("(" + recv + " ?? " + captureExprText(es, *e.args[0], 0) + "())");
            return;
        }
        if (e.name == "get") { es.w(recv + "!"); return; }
        if (e.name == "ofNullable" || e.name == "of") {
            if (!e.args.empty()) emitExpr(es, *e.args[0], 0);
            return;
        }
    }
    if (rname == "IEnumerable" || rname == "List" || rname == "LinkedList" ||
        rname == "ICollection" || rname == "HashSet" || rname == "SortedSet") {
        static const std::map<std::string, std::string> linq1 = {
            {"filter", "Where"}, {"map", "Select"}, {"mapToInt", "Select"},
            {"mapToLong", "Select"}, {"mapToDouble", "Select"}, {"mapToObj", "Select"},
            {"flatMap", "SelectMany"}, {"distinct", "Distinct"}, {"sorted", "OrderBy"},
            {"limit", "Take"}, {"skip", "Skip"}, {"anyMatch", "Any"}, {"allMatch", "All"},
            {"noneMatch", "All"},
            {"findFirst", "FirstOrDefault"}, {"findAny", "FirstOrDefault"},
            {"count", "Count"}, {"sum", "Sum"}, {"average", "Average"}, {"max", "Max"},
            {"min", "Min"}, {"reduce", "Aggregate"}, {"forEach", "ToList().ForEach"}};
            if (e.name == "collect" && !e.args.empty() && e.args[0]->k == EK::Call) {
                Expr& coll = *e.args[0];
                std::string cp = pathText(*coll.obj);
                if (coll.name == "toList" && (cp == "Collectors" || cp == "java.util.stream.Collectors")) {
                    es.w(recv + ".ToList()");
                    return;
                }
                if (coll.name == "toSet" && (cp == "Collectors" || cp == "java.util.stream.Collectors")) {
                    es.w(recv + ".ToHashSet()");
                    return;
                }
                if (coll.name == "joining") {
                    es.w("string.Join(");
                    emitArgs(es, coll.args);
                    es.w(", " + recv + ")");
                    return;
                }
                if (coll.name == "toMap") {
                    es.w(recv + ".ToDictionary(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
                if (coll.name == "counting" && coll.args.empty()) {
                    es.w(recv + ".Count()");
                    return;
                }
                if (coll.name == "summingInt" || coll.name == "summingLong" ||
                    coll.name == "summingDouble") {
                    es.w(recv + ".Sum(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
                if (coll.name == "averagingDouble" || coll.name == "averagingInt") {
                    es.w(recv + ".Average(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
                if (coll.name == "groupingBy") {
                    es.w(recv + ".ToLookup(");
                    emitArgs(es, coll.args);
                    es.w(")");
                    return;
                }
            }
            if (e.name == "collect") {
                es.w(recv + ".ToList()");
                return;
            }
        auto it = linq1.find(e.name);
        if (it != linq1.end()) {
            std::string m = it->second;
            if (e.name == "forEach") {
                if (rname == "List" || rname == "LinkedList") m = "ForEach";
                else {
                    es.ctx.needLinq = true;
                    m = "ToList().ForEach";
                }
            }
            if (e.name == "forEach") es.ctx.needLinq = true;
            if (e.name == "sorted" && e.args.size() == 1) {
                TypeInfo et = elemTypeOf(rt);
                if (et.known) {
                    es.w(recv + ".OrderBy(x => x, Comparer<" + et.text() + ">.Create(");
                    emitExpr(es, *e.args[0], 0);
                    es.w("))");
                    return;
                }
            }
            if (e.name == "noneMatch" && e.args.size() == 1 && e.args[0]->k == EK::Lambda &&
                !e.args[0]->lbodyIsBlock) {
                es.ctx.needLinq = true;
                es.w(recv + ".All(");
                Expr& lam = *e.args[0];
                if (lam.lparams.size() == 1) es.w(lam.lparams[0].second);
                else es.w("x");
                es.w(" => !(");
                emitExpr(es, *lam.lbody, 0);
                es.w("))");
                return;
            }
            if (e.name == "sorted" && e.args.empty()) {
                es.ctx.needLinq = true;
                es.w(recv + ".OrderBy(x => x)");
                return;
            }
            es.ctx.needLinq = true;
            es.w(recv + "." + m + "(");
            emitArgs(es, e.args);
            es.w(")");
            return;
        }
        if (e.name == "size") { es.w(recv + ".Count()"); es.ctx.needLinq = true; return; }
        if (e.name == "isEmpty") { es.w("(!" + recv + ".Any())"); es.ctx.needLinq = true; return; }
        if (e.name == "toArray") { es.w(recv + ".ToArray()"); es.ctx.needLinq = true; return; }
        if (e.name == "iterator") { es.w(recv + ".GetEnumerator()"); return; }
    }
    if (rname == "Thread") {
        if (e.name == "start") { es.w(recv + ".Start()"); return; }
        if (e.name == "join") { es.w(recv + ".Join("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "getName") { es.w(recv + ".Name"); return; }
        if (e.name == "setName") {
            es.w("(" + recv + ".Name = " + captureExprText(es, *e.args[0], 0) + ")");
            return;
        }
        if (e.name == "interrupt") { es.w(recv + ".Interrupt()"); return; }
        if (e.name == "isAlive") { es.w(recv + ".IsAlive"); return; }
    }
    if (rname == "Random") {
        if (e.name == "nextInt") {
            if (e.args.empty()) es.w(recv + ".Next()");
            else { es.w(recv + ".Next("); emitArgs(es, e.args); es.w(")"); }
            return;
        }
        if (e.name == "nextLong") { es.w(recv + ".NextInt64()"); return; }
        if (e.name == "nextDouble") { es.w(recv + ".NextDouble()"); return; }
        if (e.name == "nextBoolean") { es.w("(" + recv + ".Next(2) == 0)"); return; }
        if (e.name == "nextGaussian") { es.w(recv + ".NextDouble()"); return; }
    }
    if (rname == "Scanner") {
        if (e.name == "nextLine") { es.w("Console.ReadLine()"); return; }
        if (e.name == "nextInt") { es.w("int.Parse(Console.ReadLine())"); return; }
        if (e.name == "nextLong") { es.w("long.Parse(Console.ReadLine())"); return; }
        if (e.name == "nextDouble") { es.w("double.Parse(Console.ReadLine())"); return; }
        if (e.name == "nextBoolean") { es.w("bool.Parse(Console.ReadLine())"); return; }
        if (e.name == "next") { es.w("Console.ReadLine()"); return; }
        if (e.name == "hasNextLine" || e.name == "hasNext") {
            es.w("(Console.In.Peek() != -1)");
            return;
        }
        if (e.name == "close") { es.w(recv); return; }
    }
    if (rname == "string" || rname == "StreamReader" || rname == "StringReader" ||
        rname == "StreamWriter" || rname == "StringWriter") {
        if ((rname == "StreamReader" || rname == "StringReader") && e.name == "readLine") { es.w(recv + ".ReadLine()"); return; }
        if ((rname == "StreamReader" || rname == "StringReader") && e.name == "read") { es.w(recv + ".Read()"); return; }
        if (rname == "StreamWriter") {
            if (e.name == "write") { es.w(recv + ".Write("); emitArgs(es, e.args); es.w(")"); return; }
            if (e.name == "println") { es.w(recv + ".WriteLine("); emitArgs(es, e.args); es.w(")"); return; }
            if (e.name == "flush") { es.w(recv + ".Flush()"); return; }
            if (e.name == "close") { es.w(recv + ".Close()"); return; }
        }
    }
    if (rname == "Exception" || endsWith(rname, "Exception")) {
        bool userOwns =
            es.ctx.instanceMethods.count(rname + "." + e.name + "/" + std::to_string(e.args.size())) ||
            es.ctx.staticMethods.count(rname + "." + e.name + "/" + std::to_string(e.args.size()));
        if (!userOwns) {
            if (e.name == "getMessage" || e.name == "getLocalizedMessage") { es.w(recv + ".Message"); return; }
            if (e.name == "printStackTrace") {
                es.w("Console.Error.WriteLine(" + recv + ")");
                return;
            }
            if (e.name == "getCause") { es.w(recv + ".InnerException"); return; }
            if (e.name == "getStackTrace") { es.w(recv + ".StackTrace"); return; }
            if (e.name == "getClass") { es.w(recv + ".GetType()"); return; }
            if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        }
    }
    if (rname == "Guid") {
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
    }
    if (rname == "BigInteger") {
        if (e.name == "add") { es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "subtract") { es.w("(" + recv + " - " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "multiply") { es.w("(" + recv + " * " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "divide") { es.w("(" + recv + " / " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "mod") { es.w("(" + recv + " % " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "negate") { es.w("(-" + recv + ")"); return; }
        if (e.name == "abs") { es.w("BigInteger.Abs(" + recv + ")"); return; }
        if (e.name == "pow") { es.w("BigInteger.Pow(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "gcd") { es.w("BigInteger.GreatestCommonDivisor(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "equals") { es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "intValue") { es.w("((int)" + recv + ")"); return; }
        if (e.name == "longValue") { es.w("((long)" + recv + ")"); return; }
        if (e.name == "doubleValue") { es.w("((double)" + recv + ")"); return; }
        if (e.name == "intValueExact") { es.w("((int)" + recv + ")"); return; }
        if (e.name == "longValueExact") { es.w("((long)" + recv + ")"); return; }
        if (e.name == "sign") { es.w(recv + ".Sign"); return; }
        if (e.name == "shiftLeft") { es.w("(" + recv + " << " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "shiftRight") { es.w("(" + recv + " >> " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "and") { es.w("(" + recv + " & " + captureExprText(es, *e.args[0], P_BITAND) + ")"); return; }
        if (e.name == "or") { es.w("(" + recv + " | " + captureExprText(es, *e.args[0], P_BITOR) + ")"); return; }
        if (e.name == "xor") { es.w("(" + recv + " ^ " + captureExprText(es, *e.args[0], P_XOR) + ")"); return; }
        if (e.name == "not") { es.w("(~" + recv + ")"); return; }
    }
    if (rname == "decimal") {
        if (e.name == "add") { es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "subtract") { es.w("(" + recv + " - " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "multiply") { es.w("(" + recv + " * " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "divide") { es.w("(" + recv + " / " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "equals") { es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "toString") { es.w(recv + ".ToString()"); return; }
        if (e.name == "intValue") { es.w("((int)" + recv + ")"); return; }
        if (e.name == "longValue") { es.w("((long)" + recv + ")"); return; }
        if (e.name == "doubleValue") { es.w("((double)" + recv + ")"); return; }
        if (e.name == "setScale") {
            if (e.args.size() == 2) {
                es.w("Math.Round(" + recv + ", " + captureExprText(es, *e.args[0], 0) +
                     ", MidpointRounding.AwayFromZero)");
                return;
            }
            es.w("Math.Round(" + recv + ", " + captureExprText(es, *e.args[0], 0) +
                 ", MidpointRounding.AwayFromZero)");
            return;
        }
        if (e.name == "abs") { es.w("Math.Abs(" + recv + ")"); return; }
        if (e.name == "signum") { es.w("Math.Sign(" + recv + ")"); return; }
        if (e.name == "max") { es.w("Math.Max(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
        if (e.name == "min") { es.w("Math.Min(" + recv + ", " + captureExprText(es, *e.args[0], 0) + ")"); return; }
    }
    if (rname == "DateTime" || rname == "DateOnly" || rname == "TimeOnly" ||
        rname == "DateTimeOffset") {
        if (e.name == "plusDays") { es.w(recv + ".AddDays("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusWeeks") { es.w(recv + ".AddDays("); emitExpr(es, *e.args[0], 0); es.w(" * 7)"); return; }
        if (e.name == "plusMonths") { es.w(recv + ".AddMonths("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusYears") { es.w(recv + ".AddYears("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusHours") { es.w(recv + ".AddHours("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusMinutes") { es.w(recv + ".AddMinutes("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusSeconds") { es.w(recv + ".AddSeconds("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "plusNanos") { es.w(recv + ".AddMilliseconds("); emitExpr(es, *e.args[0], 0); es.w(" / 1e6)"); return; }
        if (e.name == "minusDays") { es.w(recv + ".AddDays(-("); emitArgs(es, e.args); es.w("))"); return; }
        if (e.name == "minusMonths") { es.w(recv + ".AddMonths(-("); emitArgs(es, e.args); es.w("))"); return; }
        if (e.name == "minusYears") { es.w(recv + ".AddYears(-("); emitArgs(es, e.args); es.w("))"); return; }
        if (e.name == "getYear") { es.w(recv + ".Year"); return; }
        if (e.name == "getMonthValue") { es.w(recv + ".Month"); return; }
        if (e.name == "getDayOfMonth") { es.w(recv + ".Day"); return; }
        if (e.name == "getDayOfWeek") { es.w(recv + ".DayOfWeek"); return; }
        if (e.name == "getHour") { es.w(recv + ".Hour"); return; }
        if (e.name == "getMinute") { es.w(recv + ".Minute"); return; }
        if (e.name == "getSecond") { es.w(recv + ".Second"); return; }
        if (e.name == "isBefore") { es.w("(" + recv + " < " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "isAfter") { es.w("(" + recv + " > " + captureExprText(es, *e.args[0], P_REL) + ")"); return; }
        if (e.name == "isEqual") { es.w("(" + recv + " == " + captureExprText(es, *e.args[0], P_EQ) + ")"); return; }
        if (e.name == "format") {
            if (!e.args.empty() && isStringLit(*e.args[0])) {
                es.w(recv + ".ToString(\"" + escapeCsStr(translateDatePattern(e.args[0]->val)) + "\")");
            } else {
                es.w(recv + ".ToString(");
                emitArgs(es, e.args);
                es.w(")");
            }
            return;
        }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "toEpochMilli") { es.w(recv + ".ToUnixTimeMilliseconds()"); return; }
        if (e.name == "getEpochSecond") { es.w(recv + ".ToUnixTimeSeconds()"); return; }
    }
    if (rname == "TimeSpan") {
        if (e.name == "toMillis") { es.w("((long)" + recv + ".TotalMilliseconds)"); return; }
        if (e.name == "toSeconds") { es.w("((long)" + recv + ".TotalSeconds)"); return; }
        if (e.name == "getSeconds") { es.w("((long)" + recv + ".TotalSeconds)"); return; }
        if (e.name == "getMinutes") { es.w(recv + ".Minutes"); return; }
        if (e.name == "getHours") { es.w(recv + ".Hours"); return; }
        if (e.name == "getDays") { es.w(recv + ".Days"); return; }
        if (e.name == "isZero") { es.w("(" + recv + " == TimeSpan.Zero)"); return; }
        if (e.name == "isNegative") { es.w("(" + recv + " < TimeSpan.Zero)"); return; }
        if (e.name == "abs") { es.w(recv + ".Duration()"); return; }
        if (e.name == "compareTo") { es.w(recv + ".CompareTo("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "multipliedBy") { es.w("(" + recv + " * " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "dividedBy") { es.w("(" + recv + " / " + captureExprText(es, *e.args[0], P_MUL) + ")"); return; }
        if (e.name == "plus") { es.w("(" + recv + " + " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "minus") { es.w("(" + recv + " - " + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
    }
    if (rname == "KeyValuePair") {
        if (e.name == "getKey") { es.w(recv + ".Key"); return; }
        if (e.name == "getValue") { es.w(recv + ".Value"); return; }
    }
    static const std::set<std::string> samCallNames = {
        "run", "get", "call", "apply", "accept", "test", "compare",
        "getAsInt", "getAsLong", "getAsDouble", "getAsBoolean"};
    bool isDelegateType = rname == "Action" || startsWith(rname, "Func") ||
                          rname == "Predicate" || startsWith(rname, "Predicate<") ||
                          startsWith(rname, "Comparison");
    if (isDelegateType && samCallNames.count(e.name)) {
        es.w(recv + ".Invoke(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    if (rname == "Match") {
        if (e.name == "group") {
            if (e.args.empty()) es.w(recv + ".Value");
            else {
                TypeInfo at = inferExprType(es, *e.args[0]);
                if (at.known && at.name == "string")
                    es.w(recv + ".Groups[" + captureExprText(es, *e.args[0], 0) + "].Value");
                else es.w(recv + ".Groups[" + captureExprText(es, *e.args[0], 0) + "].Value");
            }
            return;
        }
        if (e.name == "start") { es.w(recv + ".Index"); return; }
        if (e.name == "end") { es.w("(" + recv + ".Index + " + recv + ".Length)"); return; }
        if (e.name == "matches" || e.name == "find") { es.w(recv + ".Success"); return; }
    }
    if (rname == "Regex") {
        if (e.name == "matcher") { es.w(recv + ".Match("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "split") { es.w(recv + ".Split("); emitArgs(es, e.args); es.w(")"); return; }
        if (e.name == "pattern") { es.w(recv + ".ToString()"); return; }
    }
    if (rname == "StreamReader" || rname == "StreamWriter" || rname == "TextWriter") {
        if (e.name == "close") { es.w(recv + ".Close()"); return; }
        if (e.name == "flush") { es.w(recv + ".Flush()"); return; }
    }
    if (e.name == "getAnnotation" && e.args.size() == 1 && e.args[0]->k == EK::ClassLit) {
        std::string an = csTypeStr(es.ctx, e.args[0]->ty, true);
        es.ctx.needReflection = true;
        es.w("((" + an + ")" + recv + ".GetCustomAttribute(typeof(" + an + ")))");
        return;
    }
    if (e.name == "compareTo" && e.args.size() == 1) {
        es.w(recv + ".CompareTo(" + captureExprText(es, *e.args[0], P_ADD) + ")");
        return;
    }
    if (e.name == "equals" && e.args.size() == 1) {
        es.w(recv + ".Equals(" + captureExprText(es, *e.args[0], P_ADD) + ")");
        return;
    }
    if (e.name == "hashCode" && e.args.empty()) {
        es.w(recv + ".GetHashCode()");
        return;
    }
    if (e.name == "toString" && e.args.empty()) {
        es.w(recv + ".ToString()");
        return;
    }
    if (e.name == "getClass" && e.args.empty()) {
        es.w(recv + ".GetType()");
        return;
    }
    if (!rt.known) {
        if (e.name == "length" && e.args.empty()) { es.w(recv + ".Length"); return; }
        if (e.name == "size" && e.args.empty()) { es.ctx.needLinq = true; es.w(recv + ".Count()"); return; }
        if (e.name == "isEmpty" && e.args.empty()) { es.ctx.needLinq = true; es.w("(!" + recv + ".Any())"); return; }
        if ((e.name == "intValue" || e.name == "intValueExact") && e.args.empty()) { es.w("((int)" + recv + ")"); return; }
        if ((e.name == "longValue" || e.name == "longValueExact") && e.args.empty()) { es.w("((long)" + recv + ")"); return; }
        if (e.name == "doubleValue" && e.args.empty()) { es.w("((double)" + recv + ")"); return; }
        if (e.name == "floatValue" && e.args.empty()) { es.w("((float)" + recv + ")"); return; }
        if (e.name == "contains" && e.args.size() == 1) { es.w(recv + ".Contains(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "startsWith" && e.args.size() == 1) { es.w(recv + ".StartsWith(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "endsWith" && e.args.size() == 1) { es.w(recv + ".EndsWith(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "toUpperCase" && e.args.empty()) { es.w(recv + ".ToUpper()"); return; }
        if (e.name == "toLowerCase" && e.args.empty()) { es.w(recv + ".ToLower()"); return; }
        if (e.name == "trim" && e.args.empty()) { es.w(recv + ".Trim()"); return; }
        if (e.name == "strip" && e.args.empty()) { es.w(recv + ".Trim()"); return; }
        if (e.name == "toString" && e.args.empty()) { es.w(recv + ".ToString()"); return; }
        if (e.name == "hashCode" && e.args.empty()) { es.w(recv + ".GetHashCode()"); return; }
        if (e.name == "getClass" && e.args.empty()) { es.w(recv + ".GetType()"); return; }
        if (e.name == "isEmpty" && e.args.empty()) { es.w(recv + ".Length == 0"); return; }
        if (e.name == "compareTo" && e.args.size() == 1) { es.w(recv + ".CompareTo(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
        if (e.name == "equals" && e.args.size() == 1) { es.w(recv + ".Equals(" + captureExprText(es, *e.args[0], P_ADD) + ")"); return; }
    }
    if (rt.known && es.ctx.samDelegates.count(rt.name) &&
        es.ctx.samMethodNames[rt.name] == e.name) {
        es.w(recv + ".Invoke(");
        emitArgs(es, e.args);
        es.w(")");
        return;
    }
    es.w(recv + "." + e.name + "(");
    emitArgs(es, e.args);
    es.w(")");
}

void emitExpr(ES& es, Expr& e, int prec, const TypeInfo& expect) {
    switch (e.k) {
        case EK::Lit:
            emitLiteral(es, e, expect);
            break;
        case EK::Ident:
            emitIdent(es, e);
            break;
        case EK::This:
            es.w("this");
            break;
        case EK::Field:
            emitFieldExpr(es, e);
            break;
        case EK::Call:
            emitCall(es, e, expect);
            break;
        case EK::New:
            emitNewExpr(es, e, expect);
            break;
        case EK::NewArr:
            emitNewArrExpr(es, e, expect);
            break;
        case EK::ArrIdx:
            if (e.a) emitExpr(es, *e.a, P_POST);
            es.w("[");
            if (e.b) emitExpr(es, *e.b, 0);
            es.w("]");
            break;
        case EK::Bin:
            emitBinExpr(es, e, prec);
            break;
        case EK::Un:
            emitUnExpr(es, e, prec);
            break;
        case EK::Post:
            emitPostExpr(es, e, prec);
            break;
        case EK::Cast:
            emitCastExpr(es, e, prec, expect);
            break;
        case EK::Assign:
            emitAssignExpr(es, e, prec);
            break;
        case EK::Ternary:
            emitTernaryExpr(es, e, prec);
            break;
        case EK::Lambda:
            emitLambdaExpr(es, e);
            break;
        case EK::MRef:
            emitMRefExpr(es, e, expect);
            break;
        case EK::InstOf:
            emitInstOfExpr(es, e, prec);
            break;
        case EK::SwitchE:
            emitSwitchEExpr(es, e, expect);
            break;
        case EK::ClassLit:
            emitClassLitExpr(es, e);
            break;
        case EK::CtorCall:
            emitCtorCallExpr(es, e);
            break;
        case EK::Paren:
        case EK::TypeE:
        default:
            if (e.a) emitExpr(es, *e.a, prec);
            break;
    }
}
}

namespace js {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    if (f.bad()) return false;
    out = ss.str();
    if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB &&
        (unsigned char)out[2] == 0xBF)
        out = out.substr(3);
    return true;
}

bool writeFile(const std::string& path, const std::string& content) {
    std::filesystem::path p(path);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    f.flush();
    if (!f) return false;
    f.close();
    return static_cast<bool>(f);
}

void printUsage() {
    std::cout << PROGNAME << " " << VERSION << " - Java to C# converter\n";
    std::cout << "\n";
    std::cout << "Usage:\n";
    std::cout << "  javasharp --convert <input.java> to <output.cs> [--convert <input.java> to <output.cs> ...]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  --convert <in.java> to <out.cs>   Convert a Java source file to C#\n";
    std::cout << "                                    (the pair may be repeated)\n";
    std::cout << "  --version                         Print version information\n";
    std::cout << "  --help                            Print this help\n";
    std::cout << "\n";
    std::cout << "Exit codes:\n";
    std::cout << "  0  conversion succeeded\n";
    std::cout << "  1  conversion completed with syntax errors in the input\n";
    std::cout << "  2  usage error or input/output file error\n";
}

int convertOne(const std::string& inPath, const std::string& outPath) {
    std::string src;
    if (!readFile(inPath, src)) {
        std::cerr << PROGNAME << ": error: cannot read input file '" << inPath << "'\n";
        return 2;
    }
    Ctx ctx;
    Lex lex(src, ctx);
    lex.run();
    Parser parser(std::move(lex.toks), ctx);
    parser.ctx.comments = lex.comments;
    auto unit = parser.parseUnit();
    for (auto& e : ctx.errors) std::cerr << PROGNAME << ": " << inPath << " " << e << "\n";
    if (!unit) return 2;
    processImports(ctx, *unit);
    for (auto& d : unit->types) {
        registerDecl(ctx, *d);
        collectClassInfo(ctx, *d);
    }
    detectSamDelegates(ctx, *unit);
    hoistUnit(ctx, *unit);
    for (auto& d : unit->types) {
        registerDecl(ctx, *d);
        collectClassInfo(ctx, *d);
    }
    resolveOverrides(ctx, *unit);
    classifyTypes(ctx, *unit);
    detectSamDelegates(ctx, *unit);
    countMains(ctx, *unit);
    for (auto& d : unit->types) {
        for (auto& m : d->members)
            if (m.mk == 4 && m.nested) {
                registerDecl(ctx, *m.nested);
                collectClassInfo(ctx, *m.nested);
            }
    }
    std::string result = convertUnit(ctx, *unit);
    if (!writeFile(outPath, result)) {
        std::cerr << PROGNAME << ": error: cannot write output file '" << outPath << "'\n";
        return 2;
    }
    size_t lines = 0;
    for (char c : result)
        if (c == '\n') lines++;
    std::cout << PROGNAME << ": converted " << inPath << " -> " << outPath << " (" << lines
              << " lines";
    if (!ctx.errors.empty()) std::cout << ", " << ctx.errors.size() << " syntax errors";
    std::cout << ")\n";
    return ctx.errors.empty() ? 0 : 1;
}
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        js::printUsage();
        return args.size() == 1 ? 0 : 2;
    }
    if (!args.empty() && (args[0] == "--version" || args[0] == "-v")) {
        if (args.size() != 1) {
            std::cerr << js::PROGNAME << ": error: --version does not accept additional arguments\n";
            return 2;
        }
        std::cout << js::PROGNAME << " " << js::VERSION << "\n";
        return 0;
    }
    if (args.empty() || args[0] != "--convert") {
        std::cerr << js::PROGNAME << ": error: expected --convert\n\n";
        js::printUsage();
        return 2;
    }
    size_t i = 0;
    int converted = 0, failed = 0, partial = 0;
    while (i < args.size()) {
        if (args[i] == "--convert") i++;
        if (i >= args.size()) {
            std::cerr << js::PROGNAME << ": error: incomplete --convert command\n";
            return 2;
        }
        if (i + 2 >= args.size()) {
            std::cerr << js::PROGNAME << ": error: incomplete conversion pair near '" << args[i] << "'\n";
            std::cerr << "Expected: javasharp --convert <input.java> to <output.cs>\n";
            return 2;
        }
        const std::string& inFile = args[i];
        const std::string& toKw = args[i + 1];
        const std::string& outFile = args[i + 2];
        if (toKw != "to") {
            std::cerr << js::PROGNAME << ": error: expected 'to' between input and output, got '"
                      << toKw << "'\n";
            return 2;
        }
        int rc = js::convertOne(inFile, outFile);
        if (rc == 0) converted++;
        else if (rc == 1) partial++;
        else failed++;
        i += 3;
        if (i < args.size() && args[i] != "--convert" && i + 2 >= args.size()) {
            std::cerr << js::PROGNAME << ": error: unexpected argument '" << args[i] << "'\n";
            return 2;
        }
    }
    if (converted + partial + failed == 0) return 2;
    return failed > 0 ? 2 : (partial > 0 ? 1 : 0);
}
