# LAN-PORT-SCANNER

![C++](https://img.shields.io/badge/C++-17-blue)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![Status](https://img.shields.io/badge/status-active-success)
![License](https://img.shields.io/badge/license-MIT-green)

Быстрый многопоточный TCP-сканер портов для локальной сети с использованием неблокирующих сокетов и предварительной фильтрации хостов.

---

## 🚀 Возможности

* ⚡ Многопоточное сканирование
* 🎯 ARP + ICMP фильтрация целей
* 🧠 Атомарное распределение задач
* ⏱️ Таймауты через `select`
* 🔌 Неблокирующие сокеты
* 📜 История команд
* 🛠️ Ручной режим

---

## 🧠 Как это работает

```text
User Input
   ↓
Network Detection (IP + Subnet)
   ↓
ARP Scan → Devices in LAN
   ↓
ICMP Scan → Alive Hosts
   ↓
Target Selection (/a | /0 | manual)
   ↓
Task Distribution (std::atomic)
   ↓
Thread Pool
   ↓
Non-blocking connect()
   ↓
select(timeout)
   ↓
Result (OPEN / CLOSED)
```

---

## 📊 Data Flow Diagram

![Data Flow](./docs/dataflow.png)

<img width="1448" height="1086" alt="scan43" src="https://github.com/user-attachments/assets/a9674661-e844-4e05-8bbf-bd0aa275ff08" />

---

## ⚙️ Сборка

### 🔹 Windows (MinGW / MSVC)

```bash
g++ portscan.cpp -o portscan -lws2_32
```

---

## ▶️ Запуск

```bash
./portscan
```

---

## 💻 Команды

| Команда    | Описание                   |
| ---------- | -------------------------- |
| `/all`     | Сканировать всю подсеть    |
| `/0`       | Только ICMP-активные хосты |
| `/manual`  | Ввести IP вручную          |
| `/history` | История                    |
| `/clear`   | Очистить историю           |
| `/help`    | Помощь                     |
| `/exit`    | Выход                      |

---

## 🔍 Примеры использования

### Скан всей сети

```text
> /all
Сканирование подсети 192.168.0.1 - 192.168.0.255...
```

---

### Быстрое сканирование (только живые)

```text
> /0
Используются только ICMP-активные хосты...
```

---

### Ручной ввод

```text
> /manual
Введите IP: 192.168.0.100
```

---

## ⚡ Технические детали

### Сетевые функции

* `getaddrinfo` — DNS-resolving
* `socket` — создание TCP-сокета
* `connect` — подключение
* `select` — таймаут

---

### Параллелизм

* `std::thread` — пул потоков
* `std::atomic` — распределение задач

---

## 🔥 Оптимизации

* ❌ Нет блокирующих `connect`
* ⏳ Таймаут на порт (≈250ms)
* 🧹 Предварительная фильтрация хостов
* ⚙️ Нет mutex — только atomic

---

## ⚠️ Ограничения

* ICMP может блокироваться фаерволом
* ARP работает только в локальной сети
* Только IPv4
* Возможны ложные отрицательные результаты
