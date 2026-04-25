/*
Справка
std::atomic — счётчики, к которым безопасно обращаться из разных потоков без отдельного mutex (операции типа fetch_add атомарны).
std::atomic<uint64_t> — для общей очереди задач «сканирование (хост, порт)»; 64 бита хватает на произведение числа хостов и портов.
std::mutex — взаимное исключение: только один поток одновременно может держать lock (здесь — для вывода в консоль).
std::lock_guard — RAII-обёртка: при создании захватывает mutex, при уничтожении (конец блока) отпускает.
std::thread — поток выполнения ОС; join() в main ждёт, пока поток завершится.
std::vector — динамический массив; здесь хранятся объекты thread и строки IP-адресов подсети.

Скан «всей сети» в одной подсети IPv4 (/24):
Берётся локальный адрес (getLocalIP), из него извлекаются три старших октета (например 192.168.0 из 192.168.0.10).
Формируется список целей x.y.z.1 … x.y.z.255 — это один логический сегмент «все хосты в одной подсети», как в примере 192.168.0.1–255.
Сканирование чужих хостов допустимо только в сетях, где у вас есть право на такие проверки.

Раздача работ между потоками (без дублирования пар хост/порт):
Полное число задач = (число IP) * (число портов на хост). Нумеруем задачи 0 … N-1.
Поток атомарно увеличивает общий счётчик задач; по номеру задачи вычисляются: индекс хоста = task / kTcpPortCount, порт = kFirstTcpPort + task % kTcpPortCount (диапазон портов задаётся константами).
Так каждая пара (хост, порт) обрабатывается ровно один раз при любом числе воркеров.

Определение «сервиса» по TCP-порту:
Имя сервиса — условное сопоставление номера порта с типичным приложением (http, ssh и т.д.).
Вариант 1: таблица известных портов (switch/функция) — не зависит от файла services в ОС.
Вариант 2: getservbyport(htons(port), "tcp") — читает базу имён из системы; может не знать нестандартные порты.
Здесь используется сначала таблица частых портов, затем при отсутствии — попытка getservbyport; иначе подпись "unknown".

Winsock / заголовки:
winsock2.h — API сокетов Windows (socket, connect, select и т.д.); подключать до windows.h, если он есть.
ws2tcpip.h — дополнения: getaddrinfo, freeaddrinfo, InetPton, InetNtop и константы вроде INET_ADDRSTRLEN.
#pragma comment(lib, "ws2_32.lib") — подсказка линкеру: прилинковать библиотеку реализации сокетов (ws2_32).

Инициализация Winsock:
WSAStartup — обязательный первый вызов перед любой работой с сокетами; передаётся желаемая версия (MAKEWORD(2,2) = 2.2).
WSACleanup — парный вызов в конце: освобождает ресурсы Winsock в процессе.
WSADATA — структура, куда WSAStartup записывает сведения о загруженной реализации (здесь достаточно факта успеха).

Имя хоста и DNS/адреса:
gethostname — записывает в буфер сетевое имя локального компьютера (как его знает ОС).
getaddrinfo — универсальный резолвер: по имени хоста и/или подсказкам (hints) возвращает список addrinfo (адреса, тип сокета).
addrinfo — узел связного списка: ai_addr указывает на sockaddr; для IPv4 это можно трактовать как sockaddr_in; ai_next — следующий адрес.
freeaddrinfo — освобождает память, выделенную под результат getaddrinfo (обязательно после использования списка).
hints.ai_family = AF_INET — запрашиваем только IPv4.
hints.ai_socktype = SOCK_STREAM — ориентир на TCP (потоковый сокет).

Текстовый IP <-> двоичный формат:
InetPtonA — преобразует строку IPv4/IPv6 в двоичный вид (для AF_INET результат кладётся в in_addr, здесь в sin_addr).
InetNtopA — обратная операция: двоичный IPv4 из in_addr (или IPv6) в строку, например "192.168.1.1"; INET_ADDRSTRLEN — достаточный размер буфера для IPv4-строки с нулём.

Разбор и сборка строки IPv4 (MSVC):
sscanf_s — безопасный разбор формата из C-строки; здесь извлечь три октета префикса подсети из локального IP.
sprintf_s — форматирование в буфер с проверкой размера; для сборки адресов x.y.z.last при переборе last в 1…255.

Строки и копирование (MSVC):
strncpy_s — безопасное копирование с указанием размера буфера; _TRUNCATE — обрезать, если не влезает (без переполнения).

Сокет: создание и адрес назначения:
socket(AF_INET, SOCK_STREAM, 0) — IPv4, TCP, протокол по умолчанию для потока; возвращает дескриптор SOCKET или INVALID_SOCKET.
sockaddr_in — структура адреса IPv4: sin_family = AF_INET, sin_port = номер порта в сетевом порядке байт (htons), sin_addr — адрес.
htons — переводит 16-битное число из порядка байт хоста в сетевой порядок (для порта обязательно).

Подключение и неблокирующий режим:
connect — начинает TCP-подключение к указанному адресу:порту; в неблокирующем режиме часто возвращает ошибку «в процессе» (см. ниже).
ioctlsocket(sock, FIONBIO, &1) — включает неблокирующий режим: вызовы не ставят поток в сон на долгое ожидание.
WSAGetLastError — код последней ошибки Winsock после неуспешного вызова (смысл кодов — в документации MSVC).
WSAEWOULDBLOCK / WSAEINPROGRESS — для неблокирующего connect означает «операция ещё не завершена», нужно ждать готовности через select.

Ожидание с таймаутом:
select — ждёт, пока среди указанных сокетов не произойдёт событие (здесь — готовность к записи после connect) или истечёт timeval.
fd_set, FD_ZERO, FD_SET — набор дескрипторов для select; первый аргумент select в Windows игнорируется (оставляют 0).
timeval — tv_sec и tv_usec задают максимальное время ожидания (здесь из kConnectTimeoutMs).

Итог успешности connect:
getsockopt(..., SOL_SOCKET, SO_ERROR, ...) — после завершения асинхронного connect читает отложенную ошибку установления соединения; 0 — успех.

Закрытие:
closesocket — закрывает сокет (как close на Unix; на Windows для сокетов используют именно closesocket).

Параллельность:
fetch_add для uint64_t — атомарно: взять номер задачи и перейти к следующей; без дублей между воркерами.
memory_order_relaxed — минимальные гарантии порядка для атомиков (достаточно для счётчика задач в этом примере).

Прочее:
constexpr — константы времени компиляции (здесь границы портов и настройки).
reinterpret_cast — низкоуровневое приведение указателей (например sockaddr_in* к sockaddr* для connect/getsockopt).
strcmp — сравнение C-строк; здесь отфильтровать "127.0.0.1" при выборе «внешнего» локального адреса.
*/
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <chrono>
using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::thread;
using std::atomic;
using std::mutex;
using std::lock_guard;
using std::queue;
using std::pair;
using std::getline;
using std::istringstream;
using std::sort;
using std::find;
using std::unique;
using std::min;
using std::max;
using std::to_string;
using std::vector;
using std::string;
using std::memory_order_relaxed;
static void setupConsoleUtf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
static atomic<bool> g_stop{ false };
static queue<string> printQueue;
static mutex queueMtx;
pair<string, uint8_t> getLocalNetworkInfo() {
    string ip = "127.0.0.1";
    uint8_t prefixLen = 24;
    ULONG bufSize = 0;
    DWORD ret = GetAdaptersAddresses(AF_INET,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, nullptr, &bufSize);
    if (ret != ERROR_BUFFER_OVERFLOW) return { ip, prefixLen };
    vector<BYTE> buffer(bufSize);
    PIP_ADAPTER_ADDRESSES pAdapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr, pAdapter, &bufSize) != NO_ERROR) {
        return { ip, prefixLen };
    }
    for (PIP_ADAPTER_ADDRESSES pa = pAdapter; pa != nullptr; pa = pa->Next) {
        if (pa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (pa->OperStatus != IfOperStatusUp) continue;
        for (PIP_ADAPTER_UNICAST_ADDRESS ua = pa->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            if (sin->sin_family == AF_INET) {
                char temp[INET_ADDRSTRLEN];
                if (InetNtopA(AF_INET, &sin->sin_addr, temp, sizeof(temp))) {
                    if (strcmp(temp, "127.0.0.1") != 0) {
                        ip = temp;
                        prefixLen = ua->OnLinkPrefixLength; // настоящая маска
                        return { ip, prefixLen };
                    }
                }
            }
        }
    }
    return { ip, prefixLen };
}

vector<string> buildSubnetHosts(const string& localIpStr, uint8_t prefixLen) {
    vector<string> hosts;
    if (prefixLen == 0 || prefixLen > 32) prefixLen = 24;

    // ЖЁСТКОЕ ОГРАНИЧЕНИЕ — чтобы не генерировать миллионы адресов
    if (prefixLen < 16) {
        cout << "Внимание: подсеть /" << (int)prefixLen << " слишком большая. Ограничиваем сканирование до ~1024 адресов вокруг вашего IP.\n";
        prefixLen = 22;  // максимум ~1024 хоста
    }

    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf_s(localIpStr.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        hosts.push_back(localIpStr);
        return hosts;
    }

    uint32_t ip = (a << 24) | (b << 16) | (c << 8) | d;
    uint32_t mask = ~0U << (32 - prefixLen);
    uint32_t network = ip & mask;
    uint32_t hostCount = 1U << (32 - prefixLen);

    if (hostCount > 2048) hostCount = 512;   // жёсткий лимит

    hosts.reserve(hostCount);
    for (uint32_t i = 1; i < hostCount; ++i) {
        uint32_t hostAddr = network | i;
        char buf[INET_ADDRSTRLEN];
        sprintf_s(buf, sizeof(buf), "%u.%u.%u.%u",
            (hostAddr >> 24) & 0xFF,
            (hostAddr >> 16) & 0xFF,
            (hostAddr >> 8) & 0xFF,
            hostAddr & 0xFF);
        hosts.emplace_back(buf);
    }

    // выкидываем свой IP
    auto it = find(hosts.begin(), hosts.end(), localIpStr);
    if (it != hosts.end()) hosts.erase(it);

    return hosts;
}

static void sortIpv4Strings(vector<string>& v) {
    sort(v.begin(), v.end(), [](const string& a, const string& b) {
        unsigned a1, a2, a3, a4, b1, b2, b3, b4;
        if (sscanf_s(a.c_str(), "%u.%u.%u.%u", &a1, &a2, &a3, &a4) != 4)
            return a < b;
        if (sscanf_s(b.c_str(), "%u.%u.%u.%u", &b1, &b2, &b3, &b4) != 4)
            return a < b;
        if (a1 != b1)
            return a1 < b1;
        if (a2 != b2)
            return a2 < b2;
        if (a3 != b3)
            return a3 < b3;
        return a4 < b4;
        });
}

// ARP DISCOVERY 
static vector<string> discoverHostsArp(const vector<string>& candidates) {
    // на публичных подсетях (/8 и больших) ARP почти никогда не работает
    // поэтому делаем быстрый выход, чтобы не висеть
    if (candidates.size() > 2048) {
        cout << "ARP пропущен (подсеть слишком большая).\n";
        return {};
    }

    cout << "Попытка ARP discovery (" << candidates.size() << " адресов)...\n";

    vector<string> alive;
    mutex aliveMtx;
    atomic<size_t> nextIdx{0};

    constexpr unsigned kArpWorkers = 64;

    auto worker = [&](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            if (g_stop.load(memory_order_relaxed)) break;

            const string& ipStr = candidates[i];
            in_addr addr{};
            if (InetPtonA(AF_INET, ipStr.c_str(), &addr) != 1) continue;

            ULONG mac[2] = {0};
            ULONG macLen = 6;

            DWORD result = SendARP(addr.S_un.S_addr, 0, mac, &macLen);

            if (result == NO_ERROR && macLen >= 6) {
                lock_guard<mutex> lk(aliveMtx);
                alive.push_back(ipStr);
            }
        }
    };

    vector<thread> threads;
    threads.reserve(kArpWorkers);

    size_t chunk = (candidates.size() + kArpWorkers - 1) / kArpWorkers;
    for (unsigned w = 0; w < kArpWorkers; ++w) {
        size_t start = w * chunk;
        size_t end = min(start + chunk, candidates.size());
        if (start >= end) break;
        threads.emplace_back(worker, start, end);
    }

    for (auto& t : threads) t.join();

    sortIpv4Strings(alive);
    cout << "ARP нашёл: " << alive.size() << " устройств.\n";
    return alive;
}
/*
static vector<string> discoverHostsArp(const vector<string>& candidates) {
    cout << "ARP отключён (подсеть слишком большая /8).\n";
    cout << "Используем только быстрый ICMP...\n";
    return {};   // пустой список - сразу перейдёт к ICMP
}
*/

static void trimStr(string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
        start++;
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        end--;
    s = s.substr(start, end - start);
}
static size_t utf8CodePointCount(const string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F) {
            i++;
            n++;
            continue;
        }
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            i += 2;
            n++;
            continue;
        }
        if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            i += 3;
            n++;
            continue;
        }
        if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            i += 4;
            n++;
            continue;
        }
        i++;
        n++;
    }
    return n;
}
static string centerLineInWidth(const string& text, size_t widthCols) {
    const size_t vis = utf8CodePointCount(text);
    if (vis >= widthCols) {
        size_t cols = 0;
        size_t cut = 0;
        for (size_t i = 0; i < text.size() && cols < widthCols; ) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            size_t step = 1;
            if (c <= 0x7F)
                step = 1;
            else if ((c & 0xE0) == 0xC0 && i + 1 < text.size())
                step = 2;
            else if ((c & 0xF0) == 0xE0 && i + 2 < text.size())
                step = 3;
            else if ((c & 0xF8) == 0xF0 && i + 3 < text.size())
                step = 4;
            if (i + step > text.size())
                break;
            i += step;
            cols++;
            cut = i;
        }
        return text.substr(0, cut);
    }
    const size_t pad = widthCols - vis;
    const size_t left = pad / 2;
    const size_t right = pad - left;
    return string(left, ' ') + text + string(right, ' ');
}
static bool ipv4StringValid(const string& ip) {
    in_addr a{};
    return InetPtonA(AF_INET, ip.c_str(), &a) == 1;
}

constexpr unsigned kPingWorkers = 64;
constexpr DWORD kIcmpTimeoutMs = 100;
static void discoverPingWorker(
    const vector<string>* candidates,
    atomic<size_t>* nextIdx,
    vector<string>* alive,
    mutex* aliveMtx
) {
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE)
        return;
    in_addr addr{};
    char sendData[4] = { 1, 2, 3, 4 };
    const DWORD replyBufSize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 16;
    vector<char> reply(replyBufSize);
    for (;;) {
        const size_t i = nextIdx->fetch_add(1);
        if (i >= candidates->size())
            break;
        const string& ip = (*candidates)[i];
        if (InetPtonA(AF_INET, ip.c_str(), &addr) != 1)
            continue;
        const IPAddr dest = addr.S_un.S_addr;
        const DWORD n = IcmpSendEcho(
            h,
            dest,
            sendData,
            sizeof(sendData),
            nullptr,
            reply.data(),
            replyBufSize,
            kIcmpTimeoutMs
        );
        if (n != 0) {
            lock_guard<mutex> lk(*aliveMtx);
            alive->push_back(ip);
        }
    }
    IcmpCloseHandle(h);
}
static vector<string> discoverHostsIcmp(const vector<string>& candidates) {
    vector<string> alive;
    mutex aliveMtx;
    atomic<size_t> next{0};
    vector<thread> threads;
    threads.reserve(kPingWorkers);
    for (unsigned w = 0; w < kPingWorkers; w++)
        threads.emplace_back(discoverPingWorker, &candidates, &next, &alive, &aliveMtx);
    for (auto& t : threads)
        t.join();
    sortIpv4Strings(alive);
    return alive;
}
static vector<string> parseManualIps(const string& line) {
    vector<string> out;
    string s = line;
    for (char& c : s) {
        if (c == ',')
            c = ' ';
    }
    istringstream iss(s);
    string tok;
    while (iss >> tok) {
        trimStr(tok);
        if (tok.empty())
            continue;
        if (ipv4StringValid(tok))
            out.push_back(tok);
    }
    sortIpv4Strings(out);
    return out;
}
static vector<size_t> parseIndexList(const string& input, size_t maxIndex) {
    vector<size_t> out;
    if (maxIndex == 0)
        return out;
    string s = input;
    for (char& c : s) {
        if (c == ',')
            c = ' ';
    }
    istringstream iss(s);
    string tok;
    while (iss >> tok) {
        trimStr(tok);
        if (tok.empty())
            continue;
        const size_t dash = tok.find('-');
        if (dash != string::npos) {
            string left = tok.substr(0, dash);
            string right = tok.substr(dash + 1);
            trimStr(left);
            trimStr(right);
            size_t a = 0, b = 0;
            if (sscanf_s(left.c_str(), "%zu", &a) != 1)
                continue;
            if (sscanf_s(right.c_str(), "%zu", &b) != 1)
                continue;
            if (a > b)
                std::swap(a, b);
            for (size_t i = a; i <= b; i++) {
                if (i >= 1 && i <= maxIndex)
                    out.push_back(i);
            }
        } else {
            size_t idx = 0;
            if (sscanf_s(tok.c_str(), "%zu", &idx) == 1 && idx >= 1 && idx <= maxIndex)
                out.push_back(idx);
        }
    }
    sort(out.begin(), out.end());
    out.erase(unique(out.begin(), out.end()), out.end());
    return out;
}
static void clearScreen() {
    system("cls");
}
static vector<string> interactiveHostSelection(
    const vector<string>& subnetAll,
    const vector<string>& icmpAlive,
    const string& localIp
) {
    for (;;) {
        clearScreen();
        constexpr size_t kMenuInner = 66;
        const string menuMargin = "                ";
        cout << menuMargin << '+' << string(kMenuInner, '-') << "+\n";
        cout << menuMargin << '|' << centerLineInWidth("LAN port scanner — выбор устройств для скана", kMenuInner)
            << "|\n";
        cout << menuMargin << '+' << string(kMenuInner, '-') << "+\n";
        cout << "Локальный IP: " << localIp << "\n";
        cout << "По ICMP ответили: " << icmpAlive.size() << " адрес(ов).\n";
        if (icmpAlive.empty()) {
            cout << "ICMP ничего не нашёл (фаервол?). Используйте /a — вся подсеть или /m — вручную.\n\n";
        }
        cout << "Рекомендация: для скорости укажите только нужные ПК (часто до ~15 в сети).\n\n";
        if (!icmpAlive.empty()) {
            cout << "Ответили на ICMP (онлайн):\n";
            for (size_t i = 0; i < icmpAlive.size(); i++)
                cout << "  " << (i + 1) << ")  " << icmpAlive[i] << "\n";
            cout << "\n";
        }
        else {
            cout << "(По ICMP хосты не найдены — файрвол может блокировать ping. Используйте /m или /a.)\n\n";
        }
        cout << "Команды:\n";
        cout << "  /0 — сканировать ВСЕ из списка ICMP (" << icmpAlive.size() << " шт.)\n";
        cout << "  /1,3,7 или /2-5  — номера из списка выше (через запятую или диапазон)\n";
        cout << "  /a — вся подсеть (" << subnetAll.size() << " адресов, может быть долго)\n";
        cout << "  /m — ввести IPv4 вручную через запятую\n";
        cout << ">> ";
        string line;
        if (!getline(cin, line))
            return {};
        trimStr(line);
        if (line.empty())
            continue;

        if (!line.empty() && line[0] == '/')
            line = line.substr(1);
        trimStr(line); 

        if (line == "a" || line == "A")
            return subnetAll;
        if (line == "m" || line == "M") {
            cout << "Введите IPv4 через запятую (например 192.168.0.1,192.168.0.10):\n>> ";
            if (!getline(cin, line))
                return {};
            vector<string> manual = parseManualIps(line);
            if (manual.empty()) {
                cout << "Нет корректных адресов.\n";
                continue;
            }
            return manual;
        }
        if (line == "0") {
            if (icmpAlive.empty()) {
                cout << "Список ICMP пуст — выберите /m или /a.\n";
                continue;
            }
            return icmpAlive;
        }
        const vector<size_t> picks = parseIndexList(line, icmpAlive.size());
        if (picks.empty()) {
            cout << "Не удалось разобрать номера.\n";
            continue;
        }
        vector<string> chosen;
        for (size_t n : picks)
            chosen.push_back(icmpAlive[n - 1]);
        sortIpv4Strings(chosen);
        return chosen;
    }
}
const char* wellKnownTcpService(int port) {
    switch (port) {
    case 20: return "ftp-data";
    case 21: return "ftp";
    case 22: return "ssh";
    case 23: return "telnet";
    case 25: return "smtp";
    case 53: return "domain";
    case 80: return "http";
    case 110: return "pop3";
    case 135: return "msrpc";
    case 139: return "netbios-ssn";
    case 143: return "imap";
    case 443: return "https";
    case 445: return "microsoft-ds";
    case 993: return "imaps";
    case 995: return "pop3s";
    case 1433: return "ms-sql-s";
    case 3306: return "mysql";
    case 3389: return "rdp";
    case 5432: return "postgresql";
    case 5900: return "vnc";
    case 8080: return "http-alt";
    default: return nullptr;
    }
}
const char* tcpServiceName(int port) {
    if (const char* s = wellKnownTcpService(port))
        return s;
    servent* se = getservbyport(htons(static_cast<u_short>(port)), "tcp");
    if (se && se->s_name)
        return se->s_name;
    return "unknown";
}
constexpr int kFirstTcpPort = 1;
constexpr int kLastTcpPort = 512;
constexpr int kTcpPortCount = kLastTcpPort - kFirstTcpPort + 1;
constexpr int kConnectTimeoutMs = 250; // было 50 мс
constexpr unsigned kWorkers = 200; // было 1024 потоков
bool tryConnectTcp(const char* ip, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return false;
    sockaddr_in target = {};
    target.sin_family = AF_INET;
    target.sin_port = htons(static_cast<u_short>(port));
    if (InetPtonA(AF_INET, ip, &target.sin_addr) != 1) {
        closesocket(sock);
        return false;
    }
    u_long nonblocking = 1;
    if (ioctlsocket(sock, FIONBIO, &nonblocking) != 0) {
        closesocket(sock);
        return false;
    }
    if (connect(sock, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0) {
        closesocket(sock);
        return true;
    }
    const int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
        closesocket(sock);
        return false;
    }
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    timeval tv = {};
    tv.tv_sec = kConnectTimeoutMs / 1000;
    tv.tv_usec = (kConnectTimeoutMs % 1000) * 1000;
    if (select(0, nullptr, &wfds, nullptr, &tv) <= 0) {
        closesocket(sock);
        return false;
    }
    int so_error = 0;
    int len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) != 0
        || so_error != 0) {
        closesocket(sock);
        return false;
    }
    closesocket(sock);
    return true;
}
string resolveIpToHostname(const string& ipStr) {
    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    // превращаем строку "192.168.1.5" в двоичный формат, понятный системе
    if (InetPtonA(AF_INET, ipStr.c_str(), &sa.sin_addr) != 1) {
        return ""; // если IP кривой
    }
    char hostName[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa),
        hostName, NI_MAXHOST,
        nullptr, 0,
        NI_NAMEREQD) == 0) {
        return string(hostName);
    }
    return ""; // имя не найдено
}
void worker(
    const vector<string>* hostList,
    atomic<uint64_t>* nextTask,
    atomic<int>* openCount,
    uint64_t totalTasks)
{
    for (;;) {
        // Если поступила команда остановки - выходим
        if (g_stop.load(memory_order_relaxed))
            return;

        // Берем следующую задачу
        const uint64_t task = nextTask->fetch_add(1, memory_order_relaxed);
        if (task >= totalTasks)
            return;

        // Печатаем прогресс каждые 100 задач
        if (task % 100 == 0) {
            string msg = "Progress: " + to_string(task) + " / " + to_string(totalTasks) + " задач\n";
            {
                lock_guard<mutex> lk(queueMtx);
                printQueue.push(move(msg));
            }
        }
        // Вычисляем IP и порт для текущей задачи
        const uint64_t hostIdx = task / static_cast<uint64_t>(kTcpPortCount);
        const int port = kFirstTcpPort + static_cast<int>(task % static_cast<uint64_t>(kTcpPortCount));
        const string& ip = (*hostList)[static_cast<size_t>(hostIdx)];
        if (tryConnectTcp(ip.c_str(), port)) {
            openCount->fetch_add(1, memory_order_relaxed);
            // запуск инструмента поиска имени по IP
            string hostName = resolveIpToHostname(ip);
            string displayIp = ip;
            // если имя нашлось (не пустое), приклеиваем его к IP
            if (!hostName.empty()) {
                displayIp += " [" + hostName + "]";
            }
            // формируем красивое сообщение для вывода
            string msg = displayIp + ":" + to_string(port) +
                " OPEN (" + tcpServiceName(port) + ")\n";
            // отправляем в очередь на печать
            {
                lock_guard<mutex> lk(queueMtx);
                printQueue.push(move(msg));
            }
        }
    }
}
static void printerThread() {
    while (!g_stop.load(memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        lock_guard<mutex> lk(queueMtx);
        while (!printQueue.empty()) {
            cout << printQueue.front() << std::flush;
            printQueue.pop();
        }
    }
    {
        lock_guard<mutex> lk(queueMtx);
        while (!printQueue.empty()) {
            cout << printQueue.front() << std::flush;
            printQueue.pop();
        }
    }
}
// визуальщина
static vector<string> commandHistory;
static void drawHeader() {
    cout << R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                              LAN PORT SCANNER                                ║
╚══════════════════════════════════════════════════════════════════════════════╝
)" << endl;
}
static void drawCommandPanel() {
    cout << R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║  Команды:                                                                    ║
║    /all      — сканировать всю подсеть                                       ║
║    /manual   — ввести IP вручную (можно несколько через запятую)             ║
║    /history  — показать историю введённых IP                                 ║
║    /clear    — очистить историю                                              ║
║    /help     — показать эту справку                                          ║
║    /exit     — выйти из программы                                            ║
╚══════════════════════════════════════════════════════════════════════════════╝
)" << endl;
}
static void showHistory() {
    if (commandHistory.empty()) {
        cout << "История пока пуста.\n";
        return;
    }
    cout << "История введённых IP:\n";
    for (size_t i = 0; i < commandHistory.size(); ++i) {
        cout << "  " << (i + 1) << ". " << commandHistory[i] << "\n";
    }
}
int main() {
    setupConsoleUtf8();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed\n";
        return 1;
    }
    auto [localIpStr, prefixLen] = getLocalNetworkInfo();
    while (true) {
        g_stop.store(false, memory_order_relaxed);
        clearScreen();
        cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        cout << "║                    LAN PORT SCANNER  —  v1.0                                 ║\n";
        cout << "║               Сканер открытых TCP-портов в локальной сети                    ║\n";
        cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n\n";
        cout << "Локальный IP: " << localIpStr << "   |   Подсеть: /" << (int)prefixLen << "\n\n";
        cout << "Введите команду:\n";
        cout << "  /all      → сканировать всю подсеть\n";
        cout << "  /manual   → ввести IP вручную\n";
        cout << "  /help     → показать справку\n";
        cout << "  /exit     → выйти\n\n";
        cout << ">> ";
        string input;
        getline(cin, input);
        trimStr(input);
        if (input.empty()) continue;

        if (input == "/exit" || input == "/quit") {
            break;
        }
        else if (input == "/help") {
            clearScreen();
            cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
            cout << "║                                   HELP                                       ║\n";
            cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n\n";
            cout << "Основные команды:\n";
            cout << "  /all      — сканировать всю текущую подсеть\n";
            cout << "  /manual   — вручную ввести IP-адреса (через запятую)\n\n";
            cout << "В режиме выбора хостов:\n";
            cout << "  0             — все хосты, найденные по ICMP\n";
            cout << "  1,3,5 или 2-7 — выбрать номера или диапазон\n";
            cout << "  a             — вся подсеть\n";
            cout << "  m             — ввести IP вручную\n\n";
            cout << "Дополнительные команды:\n";
            cout << "  /history  — показать историю\n";
            cout << "  /clear    — очистить историю\n";
            cout << "  /back     — вернуться в главное меню\n";
            cout << "  /exit     — выйти\n\n";
            cout << "Нажмите Enter для возврата...";
            cin.get();
            continue;
        }
        vector<string> hosts;
        if (input == "/all") {
            const vector<string> subnetAll = buildSubnetHosts(localIpStr, prefixLen);
            hosts = subnetAll;
            commandHistory.push_back("Сканирование всей подсети (/" + to_string(prefixLen) + ")");
        }
        else if (input == "/manual") {
            cout << "Введите IPv4 адреса через запятую:\n>> ";
            getline(cin, input);
            trimStr(input);
            hosts = parseManualIps(input);
            if (!hosts.empty()) {
                commandHistory.push_back("Ручной ввод: " + input);
            }
        }
        else if (input == "/history") {
            showHistory();
            cout << "\nНажмите Enter для продолжения...";
            cin.get();
            continue;
        }
        else if (input == "/clear") {
            commandHistory.clear();
            cout << "История очищена.\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        else {
            cout << "Неизвестная команда. Напишите /help\n";
            continue;
        }
        if (hosts.empty()) {
            cout << "Нет целей для сканирования.\n";
            continue;
        }
        const vector<string> icmpAlive = discoverHostsIcmp(hosts);
        const vector<string> selectedHosts = interactiveHostSelection(hosts, icmpAlive, localIpStr);
        if (selectedHosts.empty()) {
            continue;
        }
        g_stop.store(false, memory_order_relaxed); // добро на работу потокам
        // очистка очереди ввода
        {
            lock_guard<mutex> lk(queueMtx);
            while (!printQueue.empty()) printQueue.pop();
        }
        // запуск поток-принтера
        thread printer(printerThread);
        // копирование выбранных хостов в список для сканирования
        vector<string> scanHosts = selectedHosts;
        // сканирование
        string historyItem = "Сканирование " + to_string(scanHosts.size()) + " хостов";
        commandHistory.push_back(historyItem);
        auto selfIt = find(scanHosts.begin(), scanHosts.end(), localIpStr);
        if (selfIt != scanHosts.end()) scanHosts.erase(selfIt);

        if (scanHosts.empty()) {
            cout << "После фильтрации self-IP целей не осталось.\n";
            g_stop.store(true, memory_order_relaxed);
            printer.join();
            continue;
        }
        const uint64_t totalTasks = static_cast<uint64_t>(scanHosts.size()) * kTcpPortCount;
        cout << "\nЗапуск сканирования " << scanHosts.size() << " хостов...\n\n";
        atomic<uint64_t> nextTask{ 0 };
        atomic<int> openCount{ 0 };
        vector<thread> threads;
        threads.reserve(kWorkers);
        for (unsigned i = 0; i < kWorkers; i++) {
            threads.emplace_back(worker, &scanHosts, &nextTask, &openCount, totalTasks);
        }
        for (auto& t : threads) t.join();
        g_stop.store(true, memory_order_relaxed);
        printer.join();
        cout << "\nГотово. Найдено открытых портов: " << openCount.load() << "\n\n";
        cout << "Нажмите Enter для возврата в главное меню...";
        cin.get();
    }
    WSACleanup();
    cout << "\nДо свидания!\n";
    return 0;
}