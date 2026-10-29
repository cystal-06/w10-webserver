# BÁO CÁO ĐẶC TẢ HỆ THỐNG & CƠ SỞ KỸ THUẬT (SYSTEM SPECIFICATION DOCUMENT - SSD)
## ĐỒ ÁN NHÓM: THIẾT KẾ VÀ XÂY DỰNG MÁY CHỦ WEB ĐƠN GIẢN (SWS - SIMPLE WEB SERVER)

* **Học phần:** CS631 -- Advanced Programming in the UNIX Environment
* **Đề tài:** Group Project: Write a simple web server (`sws`)
* **Tiêu chuẩn tham chiếu:** RFC 1945 (HTTP/1.0), RFC 3875 (CGI/1.1), POSIX.1-2008, NetBSD manual `sws(1)`
* **Nền tảng mục tiêu:** NetBSD 11.0 (chuẩn tham chiếu), Linux (Ubuntu/Arch), OmniOS
* **Sinh viên thực hiện:** Nguyễn Công Đức (MSSV: 24IT340, Lớp: 24SE1) & Nhóm sinh viên

---

## PHẦN 1: TỔNG QUAN ĐỀ TÀI & PHÂN TÍCH YÊU CẦU (OVERVIEW & REQUIREMENTS)

### 1.1. Bối cảnh và Mục tiêu Đề tài
Trong kiến trúc hệ điều hành UNIX/Linux, máy chủ web (Web Server) là một dịch vụ hệ thống (System Daemon) chạy ngầm, tiếp nhận và phản hồi các yêu cầu từ mạng thông qua giao thức HTTP. Dự án yêu cầu nhóm thiết kế và hiện thực hóa chương trình máy chủ `sws` (Simple Web Server) hoàn chỉnh bằng ngôn ngữ C, tương tác trực tiếp với tầng giao vận thông qua Socket API (`socket`, `bind`, `listen`, `accept`), quản trị tiến trình qua `fork`, `setsid` và giao tiếp liên tiến trình qua `pipe`.

**Mục tiêu cốt lõi:**
1. Tuân thủ nghiêm ngặt chuẩn **HTTP/1.0 (RFC 1945)**, chỉ chấp nhận Relative URI, hỗ trợ các phương thức `GET` và `HEAD`.
2. Khả năng tương thích ngược: Tiếp nhận các yêu cầu `HTTP/1.1` từ trình duyệt hiện đại và tự động hạ cấp xử lý như `HTTP/1.0`.
3. Hoạt động như một **System Daemon** chuẩn mực: tách khỏi Terminal, chạy nền, hỗ trợ đa kết nối đồng thời.
4. Tích hợp giao diện cổng kết nối chung **CGI/1.1 (RFC 3875)** để chạy mã kịch bản động từ phía máy chủ.
5. Đảm bảo tính khả chuyển (Portability) trên 3 hệ thống: NetBSD 11.0, Linux và OmniOS.

---

### 1.2. Danh sách Yêu cầu Chức năng (Functional Requirements - FR)

Cú pháp thực thi dòng lệnh của chương trình:
```text
sws [-dh] [-c dir] [-i address] [-l file] [-p port] dir
```

* **[FR-01] Quản trị Tham số Dòng lệnh & Chế độ Chạy:**
  - `dir` (bắt buộc): Đường dẫn thư mục gốc chứa tài liệu phục vụ web (Document Root).
  - `-h`: Hiển thị tóm tắt hướng dẫn sử dụng và thoát với mã 0.
  - `-d`: Bật chế độ gỡ lỗi (Debugging Mode). Máy chủ **không** biến thành Daemon, chạy trực tiếp trên foreground của terminal, chỉ chấp nhận **1 kết nối tuần tự tại một thời điểm**, toàn bộ log được in trực tiếp ra `stdout`.
  - Mặc định (không có `-d`): Biến thành System Daemon, chạy ngầm dưới nền, ngắt liên kết terminal, sẵn sàng tiếp nhận nhiều kết nối đồng thời.
* **[FR-02] Khởi tạo Socket & Lắng nghe Mạng (Network Binding):**
  - Mặc định lắng nghe trên cổng TCP `8080`. Cho phép đổi cổng qua `-p port`.
  - Mặc định lắng nghe đồng thời trên toàn bộ các địa chỉ IPv4 (`INADDR_ANY` - `0.0.0.0`) và IPv6 (`in6addr_any` - `::`). Cho phép ràng buộc địa chỉ cụ thể qua `-i address`.
* **[FR-03] Xử lý Yêu cầu HTTP/1.0 (RFC 1945):**
  - Chỉ chấp nhận Relative URI (RFC 1945, Section 3.2.1).
  - Hỗ trợ phương thức `GET`: Trả về khối Headers và toàn bộ nội dung file (Entity Body).
  - Hỗ trợ phương thức `HEAD`: Chỉ trả về khối Headers phản hồi, không kèm theo Body.
  - Sau khi phục vụ xong yêu cầu, đóng kết nối TCP ngay lập tức.
* **[FR-04] Xử lý Tiêu đề Điều kiện `If-Modified-Since` (Caching Mechanism):**
  - Đọc và phân tích thời gian trong tiêu đề `If-Modified-Since` của client.
  - Nếu file trên máy chủ không bị chỉnh sửa sau mốc thời gian này, máy chủ phản hồi mã `304 Not Modified` và không gửi dữ liệu file.
* **[FR-05] Tự động Sinh Chỉ mục Thư mục (Directory Indexing):**
  - Nếu yêu cầu trỏ tới một thư mục:
    + Nếu có file `index.html`: Trả về nội dung file này.
    + Nếu không có `index.html`: Máy chủ tự động sinh ra trang mã HTML liệt kê toàn bộ các file và thư mục con bên trong.
    + Danh sách phải được sắp xếp theo thứ tự chữ cái (Alphanumeric order).
    + Các file ẩn (bắt đầu bằng dấu chấm `.`) bắt buộc phải bị loại bỏ.
* **[FR-06] Phân giải Thư mục Người dùng (User Directories):**
  - Nếu URI bắt đầu bằng `~<username>/...`, máy chủ tự động chuyển hướng phân giải vào thư mục `sws` trong home của user đó (`/home/<username>/sws/...`).
* **[FR-07] Thực thi Kịch bản CGI (Common Gateway Interface - RFC 3875):**
  - Kích hoạt khi có cờ `-c dir` (chỉ định thư mục CGI Root).
  - Khi URI bắt đầu bằng `/cgi-bin/...`, phần còn lại sẽ được phân giải tương đối với thư mục CGI, thực thi script và chuyển tiếp luồng dữ liệu trả về cho client.
* **[FR-08] Ghi Nhật ký Truy cập Chuẩn Apache (Logging System):**
  - Kích hoạt khi có cờ `-l file`.
  - Định dạng từng dòng log theo chuẩn biến thể Apache:
    `%a %t "%r" %>s %b`
    (Địa chỉ IP client, Thời gian GMT dạng ISO 8601, Dòng đầu tiên của request trong ngoặc kép, Mã phản hồi HTTP, Số byte dữ liệu trả về).
* **[FR-09] Hạ cấp Giao thức Tự động (HTTP/1.1 Downgrade):**
  - Khi nhận yêu cầu `HTTP/1.1`, máy chủ tự động phản hồi theo chuẩn `HTTP/1.0`.

---

### 1.3. Yêu cầu Phi Chức năng (Non-Functional Requirements - NFR)

* **[NFR-01] Khả năng Xử lý Đa kết nối (Concurrency):** Sử dụng mô hình `fork()` tạo tiến trình con độc lập phục vụ từng kết nối, xử lý tín hiệu `SIGCHLD` qua `waitpid()` tránh hoàn toàn lỗi Zombie Process.
* **[NFR-02] An toàn & Bảo mật (Security & Path Traversal):** Sử dụng hàm `realpath()` để kiểm tra ranh giới, ngăn chặn triệt để hành vi dùng `../../` để đọc file hệ thống trái phép. Trả về `403 Forbidden` hoặc `404 Not Found`.
* **[NFR-03] Tính Khả chuyển (Portability):** Viết bằng C99 chuẩn POSIX, biên dịch không lỗi cảnh báo với các cờ: `-Wall -Wextra -Werror -pedantic`.

---

## PHẦN 2: THIẾT KẾ KIẾN TRÚC HỆ THỐNG (SYSTEM ARCHITECTURE)

### 2.1. Mô hình Tiến trình (Process Lifecycle)

```
                    +-----------------------------+
                    |        sws (main.c)         |
                    | - Parse CLI Options (getopt)|
                    | - Bind Sockets (IPv4/IPv6)  |
                    | - Daemonize (if not -d)     |
                    +--------------+--------------+
                                   |
                               while(1)
                                   |
                      accept(client_connection)
                                   |
                    +--------------v--------------+
                    |       Chế độ thực thi       |
                    +--------------+--------------+
                                   |
            +----------------------+----------------------+
            | (Chế độ Daemon)                             | (Chế độ Debug -d)
    fork() tiến trình con                                 Xử lý tuần tự trên
    Phục vụ client -> exit(0)                             tiến trình chính
    Tiến trình cha tiếp tục accept                        In log ra stdout
```

---

### 2.2. Đặc tả Cấu trúc Dữ liệu Cốt lõi (Data Structures)

#### 1. Cấu trúc Cấu hình Hệ thống (`ServerConfig`)
```c
typedef struct {
    int port;                  /* Cổng lắng nghe (Mặc định: 8080) */
    char *bind_ip;             /* Địa chỉ IP ràng buộc (NULL nếu lắng nghe tất cả) */
    char *doc_root;            /* Thư mục gốc chứa web (Bắt buộc) */
    char *cgi_dir;             /* Thư mục chứa script CGI (-c) */
    char *log_file;            /* Đường dẫn file log (-l) */
    FILE *log_fp;              /* Con trỏ file stream ghi log */
    int debug_mode;            /* 1: Chế độ Debug (-d), 0: Daemon */
    int sock_v4;               /* Socket IPv4 */
    int sock_v6;               /* Socket IPv6 */
} ServerConfig;
```

#### 2. Cấu trúc Bản tin Yêu cầu (`HttpRequest`)
```c
typedef enum {
    METHOD_UNSUPPORTED = 0,
    METHOD_GET,
    METHOD_HEAD
} HttpMethod;

typedef struct {
    HttpMethod method;         /* Phương thức yêu cầu */
    char raw_method[16];       /* Chuỗi method gốc: "GET", "HEAD"... */
    char uri[2048];            /* Đường dẫn URI tài nguyên */
    char query_string[2048];   /* Chuỗi tham số sau dấu '?' (CGI) */
    char http_version[16];     /* Phiên bản: "HTTP/1.0" hoặc "HTTP/1.1" */
    char first_line[4096];     /* Dòng đầu tiên của request (ghi log) */
    time_t if_modified_since;  /* Mốc thời gian If-Modified-Since (-1 nếu không có) */
    char client_ip[INET6_ADDRSTRLEN]; /* Chuỗi IP máy khách */
} HttpRequest;
```

#### 3. Cấu trúc Bản tin Phản hồi (`HttpResponse`)
```c
typedef struct {
    int status_code;           /* Mã trạng thái: 200, 304, 400, 403, 404, 500, 501 */
    const char *status_phrase; /* Chuỗi giải thích: "OK", "Not Found"... */
    char content_type[128];    /* Kiểu MIME: text/html, image/png... (từ libmagic) */
    off_t content_length;      /* Dung lượng phản hồi tính theo byte */
    time_t last_modified;      /* Thời điểm sửa đổi file gần nhất */
    char file_path[PATH_MAX];  /* Đường dẫn vật lý của file trên ổ đĩa */
} HttpResponse;
```

---

### 2.3. Thiết kế Phân rã Module & Hợp đồng Giao tiếp (Module Interfaces)

Hệ thống được chia thành 5 module độc lập:

1. **Module 1: `network.h / network.c` (Quản trị Mạng):**
   - `int network_init(ServerConfig *config);`: Mở socket IPv4 và IPv6, gán tùy chọn `SO_REUSEADDR`, bind địa chỉ và thiết lập chế độ lắng nghe `listen()`.
   - `int network_accept(const ServerConfig *config, struct sockaddr_storage *client_addr, socklen_t *addr_len);`: Chấp nhận kết nối từ client.
2. **Module 2: `http.h / http.c` (Phân tích cú pháp & Giao thức HTTP):**
   - `int http_parse_request(int client_fd, HttpRequest *req);`: Đọc dữ liệu từ socket, phân tích Request line và Headers.
   - `void http_send_response_headers(int client_fd, const HttpResponse *res);`: Gửi các trường tiêu đề chuẩn hóa: `Date`, `Server`, `Last-Modified`, `Content-Type`, `Content-Length`.
   - `void http_send_error(int client_fd, int code, const ServerConfig *config, const HttpRequest *req);`: Gửi trang báo lỗi định dạng chuẩn (400, 403, 404, 500, 501).
3. **Module 3: `resource.h / resource.c` (Quản lý Tài nguyên & File System):**
   - `int resource_resolve_path(const char *uri, const ServerConfig *config, char *real_path, size_t max_len);`: Phân giải URI, kiểm tra bảo mật chống Path Traversal (`realpath`), xử lý `~user`.
   - `void resource_get_mime(const char *path, char *mime_buf, size_t buf_len);`: Sử dụng thư viện `libmagic` (`magic_file`) để xác định kiểu nội dung của tệp.
   - `int resource_serve_dir_index(int client_fd, const char *dir_path, const char *uri);`: Quét thư mục, sắp xếp chữ cái qua `alphasort`, lọc bỏ file `.dot`, tạo mã HTML danh mục gửi về client.
4. **Module 4: `cgi.h / cgi.c` (Động cơ Thực thi CGI):**
   - `int cgi_execute(int client_fd, const HttpRequest *req, const char *script_path, const ServerConfig *config);`: Tạo pipe, fork tiến trình con, thiết lập các biến môi trường RFC 3875, chuyển hướng luồng I/O và gọi `execve`.
5. **Module 5: `logger.h / logger.c` (Hệ thống Ghi Nhật ký):**
   - `void logger_log(const ServerConfig *config, const HttpRequest *req, int status_code, off_t bytes_sent);`: Định dạng và ghi dòng nhật ký theo chuẩn Apache `%a %t "%r" %>s %b`.

---

## PHẦN 3: ĐẶC TẢ CHI TIẾT CÁC THUẬT TOÁN CỐT LÕI

### 3.1. Thuật toán An toàn Phân giải Đường dẫn (Path Traversal Protection)
Để đảm bảo an toàn tuyệt đối cho máy chủ:
```text
Bước 1: Trích xuất URI, loại bỏ phần tham số sau dấu '?'.
Bước 2: Kiểm tra tiền tố người dùng:
        Nếu URI bắt đầu bằng "/~":
            Lấy username -> gọi getpwnam(username).
            Ghép đường dẫn: /home/<user>/sws/<subpath>.
        Ngược lại:
            Ghép đường dẫn: <doc_root>/<uri>.
Bước 3: Gọi hàm realpath(target, resolved_path) để giải mã toàn bộ symlink và chuỗi "../".
Bước 4: Kiểm tra chuỗi: resolved_path có bắt đầu bằng tiền tố doc_root (hoặc user_sws_dir) không?
        - Nếu KHÔNG: Từ chối ngay lập tức, trả về lỗi 403 Forbidden.
        - Nếu CÓ: Tiếp tục kiểm tra stat() xem là file hay thư mục.
```

---

### 3.2. Thuật toán Thực thi CGI (RFC 3875)
```text
Bước 1: Kiểm tra quyền thực thi của file script qua access(script_path, X_OK).
        Nếu không có quyền -> Trả về 403 Forbidden.
Bước 2: Khởi tạo pipe: int pfd[2]; pipe(pfd);
Bước 3: Gọi fork():
        - Tiến trình con (Child):
            + Chuyển hướng stdout sang pfd[1]: dup2(pfd[1], STDOUT_FILENO);
            + Đóng pfd[0] và pfd[1];
            + Thiết lập các biến môi trường:
                REQUEST_METHOD  = req->raw_method
                QUERY_STRING    = req->query_string
                SCRIPT_NAME     = uri
                REMOTE_ADDR     = req->client_ip
                SERVER_PORT     = config->port (chuỗi)
                SERVER_PROTOCOL = "HTTP/1.0"
                GATEWAY_INTERFACE = "CGI/1.1"
            + Gọi execve(script_path, argv, envp);
        - Tiến trình cha (Parent):
            + Đóng pfd[1];
            + Gửi Status line: "HTTP/1.0 200 OK\r\n";
            + Gửi header Server và Date;
            + Đọc từ pfd[0] và chuyển tiếp trực tiếp vào socket client_fd cho đến khi hết dữ liệu;
            + Đóng pfd[0] và gọi waitpid() để thu dọn tiến trình con.
```

---

### 3.3. Thuật toán Tự động Sinh Danh mục Thư mục (Directory Indexing)
```text
Bước 1: Gọi hàm hệ thống scandir(dir_path, &namelist, NULL, alphasort).
Bước 2: Bắt đầu tạo mã HTML với tiêu đề: "Index of <URI>".
Bước 3: Duyệt qua từng phần tử trong namelist:
        - Nếu tên file bắt đầu bằng '.' (tập tin ẩn) -> Bỏ qua.
        - Nếu là thư mục con: Hiển thị kèm dấu gạch chéo '/' ở cuối.
        - Tạo thẻ liên kết HTML: <a href="<filename>"><filename></a>.
Bước 4: Gửi dữ liệu HTML hoàn chỉnh về client kèm header Content-Type: text/html.
Bước 5: Giải phóng bộ nhớ namelist đã cấp phát bởi scandir.
```

---

### 4.1. Bảng Phân công Nhiệm vụ theo 3 Thành viên & Ràng buộc Kỹ thuật

Dự án được phân chia cho đúng **3 thành viên** theo đúng quy định đóng gói `user1-user2-user3.tar` của đề bài CS631. Trưởng nhóm (**Nguyễn Công Đức**) đảm nhiệm vai trò kiến trúc sư trưởng, chịu trách nhiệm khối lượng công việc lớn nhất (Core Engine, Daemonize, Dual-stack Network và Tích hợp hệ thống):

| Thành viên | Vai trò | Tỷ trọng | Phân hệ & Nhiệm vụ cụ thể | Ràng buộc Kỹ thuật (Technical Constraints & Contracts) |
| :--- | :--- | :---: | :--- | :--- |
| **Thành viên 1**<br>**(Nguyễn Công Đức)**<br>*Trưởng nhóm (Team Leader)* | Lead Systems Architect & Core Daemon Engine | **45%** | **1. Base Architecture & Main Flow (`main.c`):**<br>- Thiết kế cấu trúc dữ liệu chung (`ServerConfig`, `HttpRequest`, `HttpResponse`).<br>- Bộ phân tích tham số CLI `getopt` xử lý đủ 6 cờ: `-d, -h, -c dir, -i address, -l file, -p port` và đối số bắt buộc `dir`.<br>**2. System Daemon & Process Management:**<br>- Hiện thực hàm `daemonize()` chuẩn POSIX (`fork`, `setsid`, ngắt terminal, chuyển `stdin/stdout/stderr` sang `/dev/null`, `umask(027)`).<br>- Xử lý chế độ `-d`: Chạy foreground, tiếp nhận tuần tự duy nhất 1 kết nối tại một thời điểm, log ra `stdout`.<br>- Bắt tín hiệu `SIGCHLD` qua `sigaction` + `SA_RESTART` kết hợp vòng lặp `waitpid(-1, &st, WNOHANG)` triệt tiêu 100% tiến trình Zombie.<br>- Bắt `SIGINT`, `SIGTERM` dọn dẹp đóng socket và thoát sạch.<br>**3. Dual-Stack Network Engine (`network.c/.h`):**<br>- Mở song song 2 socket IPv4 (`AF_INET`) và IPv6 (`AF_INET6`), gán `SO_REUSEADDR` và `IPV6_V6ONLY`.<br>- Lắng nghe `listen()` và tiếp nhận `accept()` đa kết nối.<br>- Trích xuất IP client bằng `inet_ntop()` chuyển tiếp cho Logger và CGI.<br>**4. Tích hợp & Quản trị Dự án:**<br>- Viết `Makefile` chuẩn hóa (`all, sws, clean, test, dist`).<br>- Kiểm tra rò rỉ bộ nhớ qua `valgrind`, kiểm thử đa nền tảng (NetBSD, Linux, OmniOS), review PR và bảo vệ nhánh `main`. | - **Ràng buộc CLI:** Bắt lỗi thiếu `dir` thoát mã >0; cổng `-p` phải trong dải $[1, 65535]$; kiểm tra quyền đọc thư mục gốc qua `access(dir, R_OK)`.<br>- **Ràng buộc Daemon:** Tuyệt đối không để lại tiến trình ma (Zero Zombie), không sinh bất kỳ thông điệp nào ra terminal khi chạy ở chế độ Daemon.<br>- **Ràng buộc Socket:** Hỗ trợ đầy đủ cả IPv4 và IPv6, tự động gán địa chỉ `INADDR_ANY` và `in6addr_any` nếu không có `-i`. |
| **Thành viên 2** | HTTP Protocol & Parser Specialist | **28%** | **1. HTTP Request Parser (`http.c/.h`):**<br>- Đọc socket an toàn, phân tích cú pháp Request Line và Headers theo **RFC 1945**.<br>- Xử lý cơ chế hạ cấp âm thầm `HTTP/1.1` về `HTTP/1.0`.<br>- Phân tích tiêu đề điều kiện `If-Modified-Since` (chuyển đổi chuỗi ngày tháng GMT sang `time_t` để so sánh với `st_mtime`).<br>**2. HTTP Response Generator & Error Dispatcher:**<br>- Đóng gói 5 trường tiêu đề phản hồi bắt buộc: `Date` (GMT), `Server` (`sws/1.0`), `Last-Modified` (GMT), `Content-Type`, `Content-Length`.<br>- Xây dựng các hàm phát sinh trang mã lỗi HTML: `400 Bad Request`, `403 Forbidden`, `404 Not Found`, `500 Internal Server Error`, `501 Not Implemented`.<br>- Truyền tải nội dung file cho `GET`, chỉ gửi header cho `HEAD`, ngắt kết nối sau khi phục vụ xong. | - **Ràng buộc URI:** Chỉ chấp nhận Relative URI (RFC 1945 Mục 3.2.1). Nếu nhận Absolute URI (`http://...`) $\implies$ Trả về ngay `400 Bad Request`.<br>- **Ràng buộc Method:** Chỉ cho phép `GET` và `HEAD`. Các method khác (`POST, PUT, DELETE...`) $\implies$ Bắt buộc trả về `501 Not Implemented`.<br>- **Ràng buộc Caching:** Nếu file không bị sửa đổi sau thời điểm `If-Modified-Since` $\implies$ Trả về đúng mã `304 Not Modified`, không gửi Body. |
| **Thành viên 3** | File System, MIME, CGI & Logging Specialist | **27%** | **1. Quản lý Tài nguyên & Bảo mật (`resource.c/.h`):**<br>- Phân giải URI sang đường dẫn vật lý trên đĩa dựa trên `doc_root`.<br>- Chống tấn công Path Traversal bằng kiểm tra ranh giới tuyệt đối qua `realpath()`.<br>- Phân giải thư mục cá nhân `~user` vào `/home/<user>/sws/` bằng `getpwnam()`.<br>- Tự động sinh danh mục thư mục (Directory Indexing) bằng `scandir` + `alphasort` khi không có `index.html` (bỏ qua file `.dot`).<br>- Tích hợp thư viện `libmagic` (`magic.h`) xác định MIME-Type theo mẫu `magic(5)`.<br>**2. Động cơ Thực thi CGI (`cgi.c/.h` - RFC 3875):**<br>- Kiểm tra cờ `-c dir` và tiền tố URI `/cgi-bin/`.<br>- Thiết lập đường ống liên tiến trình (`pipe`), `fork`, `dup2` chuyển hướng luồng I/O.<br>- Thiết lập biến môi trường chuẩn RFC 3875 (`REQUEST_METHOD, QUERY_STRING, SCRIPT_NAME, REMOTE_ADDR, SERVER_PORT, SERVER_PROTOCOL, GATEWAY_INTERFACE`).<br>- Chạy script bằng `execve()`, chuyển tiếp dữ liệu về socket client.<br>**3. Module Ghi Nhật ký Truy cập (`logger.c/.h`):**<br>- Ghi log khi có cờ `-l file` (hoặc in `stdout` khi có cờ `-d`).<br>- Định dạng chuẩn biến thể Apache: `%a %t "%r" %>s %b`. | - **Ràng buộc Bảo mật:** Nghiêm cấm mọi truy cập vượt ra ngoài Document Root. Nếu phát hiện `../../` cố ý thoát ranh giới $\implies$ Trả về `403 Forbidden` hoặc `404 Not Found`.<br>- **Ràng buộc Indexing:** Danh sách file phải được sắp xếp theo đúng thứ tự bảng chữ cái; tuyệt đối không hiển thị file ẩn bắt đầu bằng dấu chấm `.`.<br>- **Ràng buộc CGI:** Phải kiểm tra quyền thực thi `access(path, X_OK)` trước khi fork; thu hồi triệt để tiến trình con qua `waitpid()`.<br>- **Ràng buộc Logging:** Ghi file nguyên tử (Atomic append với cờ `O_APPEND`), chuẩn hóa định dạng thời gian ISO 8601 GMT (`%Y-%m-%dT%H:%M:%SZ`). |

---

### 4.2. Quy chuẩn Quản lý Mã nguồn với Git (Git Workflow & Version Control)

Dự án áp dụng quy chuẩn quản lý phiên bản với Git tiêu chuẩn (hỗ trợ làm việc cục bộ hoặc đồng bộ qua kho lưu trữ cá nhân/nhóm):

1. **Khởi tạo và Cấu hình Kho lưu trữ:**
   - Khởi tạo Git repository trong thư mục dự án:
     ```bash
     git init
     git config user.name "Nguyen Cong Duc"
     git config user.email "nguyencongduc@example.com"
     ```
2. **Chiến lược Phân nhánh Tính năng (Feature Branching Strategy):**
   - **Nhánh `main`:** Lưu trữ phiên bản mã nguồn ổn định nhất, sẵn sàng đóng gói nộp bài.
   - **Các nhánh tính năng độc lập:**
     - `feature/daemon-core`: Phát triển khung sườn `main.c`, CLI parser, quy trình hóa Daemon và Socket mạng (Trưởng nhóm phụ trách).
     - `feature/http-parser`: Xây dựng bộ phân tích HTTP request, xử lý phương thức GET/HEAD và mã trạng thái.
     - `feature/resource-mgmt`: Phân giải đường dẫn tài nguyên, chống Path Traversal, Directory Indexing và `libmagic`.
     - `feature/cgi-logging`: Hiện thực động cơ chạy CGI (RFC 3875) và cơ chế ghi log chuẩn Apache.
3. **Quy chuẩn Thông điệp Commit (Meaningful Commit Messages):**
   - Tuân thủ nguyên tắc giảng viên yêu cầu: *"Commit early, commit often, communicate regularly"*.
   - Mỗi commit gắn liền với một thay đổi logic cụ thể, sử dụng tiền tố chuẩn:
     - `feat(core): setup daemon process and socket dual-stack`
     - `feat(http): parse GET and HEAD request lines`
     - `feat(resource): implement alphanumeric directory indexing`
     - `feat(cgi): support RFC 3875 environment variables and execution`
     - `fix(security): prevent directory traversal via realpath check`
4. **Trích xuất Nhật ký Nộp bài:**
   - Giảng viên sẽ chấm điểm dựa trên lịch sử commit, nhóm trích xuất file log đính kèm vào gói nộp bài bằng lệnh:
     ```bash
     git log --stat > git_log.txt
     ```

---

### 4.3. Quy chuẩn Biên dịch & Đóng gói Nộp bài (Deliverables)

* **Quy chuẩn `Makefile`:**
  - Compiler: `gcc`
  - Flags bắt buộc: `-Wall -Wextra -Werror -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700`
  - Thư viện liên kết: `-lmagic`
* **Gói nộp bài cuối cùng:**
  - Định dạng file nén: `user1-user2-user3.tar` (chứa toàn bộ thư mục cùng tên).
  - Cấu trúc thư mục:
    ```text
    user1-user2-user3/
    ├── Makefile
    ├── README
    ├── checklist
    ├── SPECIFICATION.md
    └── src/
        ├── main.c
        ├── network.c / network.h
        ├── http.c / http.h
        ├── resource.c / resource.h
        ├── cgi.c / cgi.h
        └── logger.c / logger.h
    ```

---

## PHẦN 5: MA TRẬN KIỂM THỬ CHẤP NHẬN (ACCEPTANCE TEST MATRIX)

| Mã kiểm thử | Tình huống kiểm thử | Câu lệnh thực thi | Tiêu chí chấp thuận (Pass Criteria) |
| :---: | :--- | :--- | :--- |
| **TC-01** | Trợ giúp sử dụng | `./sws -h` | In hướng dẫn sử dụng và thoát mã 0. |
| **TC-02** | Chế độ Debug cổng tùy chọn | `./sws -d -p 9090 ./www` | Chạy foreground cổng 9090, in log ra màn hình console. |
| **TC-03** | Chế độ Daemon nền | `./sws -p 8080 ./www` | Chạy ngầm lập tức, hiển thị tiến trình trong `ps -ef \| grep sws`. |
| **TC-04** | Yêu cầu GET file tĩnh | `curl -i http://localhost:8080/index.html` | Trả về `HTTP/1.0 200 OK`, `Content-Type: text/html`, đúng dữ liệu file. |
| **TC-05** | Yêu cầu HEAD | `curl -I http://localhost:8080/index.html` | Trả về khối Headers đầy đủ, không có phần Body. |
| **TC-06** | Header `If-Modified-Since` | `curl -i -H "If-Modified-Since: <future_date>" http://localhost:8080/index.html` | Trả về `HTTP/1.0 304 Not Modified`, không gửi Body. |
| **TC-07** | Directory Indexing tự động | `curl -i http://localhost:8080/docs/` | Trả về mã HTML danh sách tệp theo thứ tự chữ cái, ẩn tệp `.dot`. |
| **TC-08** | Thư mục người dùng `~user` | `curl -i http://localhost:8080/~nguyencongduc/` | Phân giải vào `/home/nguyencongduc/sws/index.html`. |
| **TC-09** | Chặn Directory Traversal | `curl -i http://localhost:8080/../../etc/passwd` | Máy chủ chặn và trả về `403 Forbidden` hoặc `404 Not Found`. |
| **TC-10** | Chạy kịch bản CGI | `curl -i http://localhost:8080/cgi-bin/test.cgi` | Thực thi script và nhận phản hồi `200 OK` do script sinh ra. |
| **TC-11** | Ghi log Apache | `./sws -l access.log ./www` | File `access.log` ghi nhận dòng log đúng định dạng `%a %t "%r" %>s %b`. |
| **TC-12** | Hạ cấp HTTP/1.1 | `curl -i --http1.1 http://localhost:8080/` | Máy chủ tự động hạ cấp và phản hồi chuẩn `HTTP/1.0`. |
| **TC-13** | Đối chiếu chuẩn `bozohttpd(8)` | Chạy song song `bozohttpd` cổng 8080 và `sws` cổng 8081 | Kết quả trả về của `sws -p 8081 -c cgi-bin docroot` tương đồng với `/usr/libexec/httpd -b -I 8080 -X -u -p sws -c cgi-bin docroot`. |

