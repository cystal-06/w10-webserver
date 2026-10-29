# BÁO CÁO THỰC HÀNH TUẦN 10: GIAO THỨC HTTP

- **Họ và tên:** Nguyễn Công Đức
- **Lớp:** 24SE1
- **Mã số sinh viên (MSSV):** 24IT340
- **Môi trường thực hành:** Linux Terminal (Arch Linux, GNOME)

---

## 1. Gửi HTTP Request thủ công qua Telnet

**Mục tiêu:** Kết nối trực tiếp đến Web Server trên cổng TCP 80, tự tay soạn thảo bản tin HTTP/1.0 Request và phân tích cấu trúc phản hồi HTTP Response.

**Lệnh kết nối:**
```bash
telnet www.google.com 80
```
*(Nếu máy chưa có lệnh `telnet`, cài nhanh bằng: `sudo pacman -S inetutils` hoặc dùng `ncat -C www.google.com 80`)*

**Nội dung Request soạn trong Terminal:**
```http
GET / HTTP/1.0
If-Modified-Since: Thu, 29 Oct 2026 00:00:00 GMT
User-Agent: NguyenCongDuc-24SE1-24IT340

```
*(Lưu ý: Sau dòng `User-Agent: ...`, nhấn **Enter 2 lần** để kết thúc phần Header)*

### Phân tích thành phần bản tin HTTP Request:
| Thành phần | Giá trị thực tế | Ý nghĩa kỹ thuật |
| :--- | :--- | :--- |
| **Request Method** | `GET` | Phương thức yêu cầu lấy tài nguyên từ server |
| **URI** | `/` | Tài nguyên gốc (Root document) |
| **Protocol Version** | `HTTP/1.0` | Phiên bản giao thức truyền vận sử dụng |
| **Request Modifiers** | `If-Modified-Since: Thu, 29 Oct 2026 00:00:00 GMT` | Điều kiện: Chỉ trả về nội dung nếu trang đã cập nhật sau mốc thời gian này |
| **Client Information** | `User-Agent: NguyenCongDuc-24SE1-24IT340` | Thông tin định danh client gửi (Họ tên - Lớp - MSSV) |

### Phân tích thành phần bản tin HTTP Response nhận được:
| Thành phần Response | Giá trị ghi nhận | Giải thích |
| :--- | :--- | :--- |
| **Status Line** | `HTTP/1.0 200 OK` | Yêu cầu thành công, tài nguyên hợp lệ |
| **Server Information** | `Server: gws` | Google Web Server |
| **Entity Meta-Information** | `Content-Type: text/html; charset=ISO-8859-1`<br>`Date: Thu, 29 Oct 2026 ...` | Dữ liệu trả về là mã HTML và thời gian phản hồi theo chuẩn GMT |
| **Content Body** | `<!doctype html>...` | Mã nguồn trang chủ hiển thị phía trình duyệt |

```
+-----------------------------------------------------------------------------------+
|                        [CHÈN ẢNH CHỤP TERMINAL 1 TẠI ĐÂY]                         |
|  (Ảnh chụp Terminal hiển thị lệnh telnet, request có User-Agent Nguyễn Công Đức   |
|   và response HTTP/1.0 200 OK cùng nội dung HTML)                                 |
+-----------------------------------------------------------------------------------+
```
*Hình 1: Thao tác gửi bản tin HTTP/1.0 thủ công bằng Telnet và phản hồi từ Google Server*

---

## 2. Khảo sát các nhóm Status Code trong HTTP

### 2.1. Phản hồi chuyển hướng Redirect 301 (Nhóm 3xx)
**Mục tiêu:** Kiểm tra phản ứng của máy chủ khi truy cập cổng HTTP bình thường của một website bắt buộc HTTPS.

**Lệnh kết nối:**
```bash
telnet www.stevens.edu 80
```

**Soạn Request:**
```http
GET / HTTP/1.1
Host: www.stevens.edu

```

**Phân tích kết quả:**
Server phản hồi mã `HTTP/1.1 301 Moved Permanently` kèm theo tiêu đề `Location: https://www.stevens.edu/`. Trình duyệt khi nhận mã này sẽ tự động chuyển hướng người dùng sang đường dẫn HTTPS an toàn.

```
+-----------------------------------------------------------------------------------+
|                        [CHÈN ẢNH CHỤP TERMINAL 2 TẠI ĐÂY]                         |
|      (Ảnh chụp terminal hiển thị mã HTTP/1.1 301 Moved Permanently và Location)   |
+-----------------------------------------------------------------------------------+
```
*Hình 2: Phản hồi mã trạng thái 301 Moved Permanently từ máy chủ stevens.edu*

---

### 2.2. Kết nối HTTPS bảo mật bằng OpenSSL s_client (Nhóm 2xx)
**Mục tiêu:** Thiết lập phiên kết nối mã hóa TLS/SSL qua cổng 443 và kiểm tra mã thành công 200 OK.

**Lệnh kết nối:**
```bash
openssl s_client -servername www.stevens.edu -connect www.stevens.edu:443
```

**Soạn Request (sau khi hoàn tất bắt tay TLS):**
```http
GET / HTTP/1.1
Host: www.stevens.edu

```

**Phân tích kết quả:**
Sau khi xác thực chuỗi chứng chỉ SSL (`Certificate chain`, `Server certificate`), ta gửi request HTTP/1.1 và server trả về `HTTP/1.1 200 OK` cùng nội dung web mã hóa được giải mã trực tiếp trên terminal.

```
+-----------------------------------------------------------------------------------+
|                        [CHÈN ẢNH CHỤP TERMINAL 3 TẠI ĐÂY]                         |
|     (Ảnh chụp quá trình bắt tay SSL/TLS và kết quả phản hồi HTTP/1.1 200 OK)      |
+-----------------------------------------------------------------------------------+
```
*Hình 3: Kết nối HTTPS qua OpenSSL s_client nhận mã trạng thái 200 OK*

---

### 2.3. Bảng tổng hợp các nhóm mã trạng thái (HTTP Status Codes)
| Nhóm mã | Tên phân loại | Ý nghĩa | Ví dụ phổ biến |
| :---: | :--- | :--- | :--- |
| **1xx** | Informational | Đã nhận yêu cầu, đang tiếp tục quá trình xử lý | 100 Continue, 101 Switching Protocols |
| **2xx** | Success | Yêu cầu đã được tiếp nhận, hiểu và thực hiện thành công | 200 OK, 201 Created, 204 No Content |
| **3xx** | Redirection | Cần thực hiện hành động chuyển tiếp (chuyển hướng URL) | 301 Moved Permanently, 304 Not Modified |
| **4xx** | Client Error | Lỗi do phía Client gửi yêu cầu không hợp lệ | 400 Bad Request, 403 Forbidden, 404 Not Found |
| **5xx** | Server Error | Máy chủ gặp lỗi không thể hoàn thành yêu cầu hợp lệ | 500 Internal Server Error, 502 Bad Gateway |

---

## 3. Khảo sát cơ chế Transfer Protocol và CGI

### 3.1. Thương lượng định dạng tài nguyên với header Accept
**Mục tiêu:** Kiểm tra cơ chế Content Negotiation (thương lượng nội dung) giữa client và server.

**Lệnh kết nối:**
```bash
telnet www.google.com 80
```

**Soạn Request:**
```http
GET / HTTP/1.0
Accept: application/json
User-Agent: NguyenCongDuc-24IT340

```

**Phân tích:**
Header `Accept: application/json` thông báo cho server biết client mong muốn dữ liệu trả về dạng JSON. Máy chủ sẽ phân tích tiêu đề này để quyết định định dạng Content-Type phù hợp nhất trả về cho client.

```
+-----------------------------------------------------------------------------------+
|                        [CHÈN ẢNH CHỤP TERMINAL 4 TẠI ĐÂY]                         |
|        (Ảnh chụp request chứa Accept: application/json và thông tin sinh viên)    |
+-----------------------------------------------------------------------------------+
```
*Hình 4: Kiểm tra cơ chế thương lượng nội dung thông qua trường tiêu đề Accept*

---

### 3.2. Kiểm tra thực thi kịch bản CGI (Common Gateway Interface)
**Mục tiêu:** Thực thi kịch bản phía server theo chuẩn RFC 3875 và kiểm tra việc truyền các biến môi trường HTTP.

**Lệnh thực thi:**
```bash
curl -v https://www.netmeister.org/cgi-bin/env.cgi
```

**Phân tích:**
Web server thực thi file script `env.cgi` và trả về kết quả là toàn bộ các biến môi trường (Environment variables) liên quan đến request như: `REQUEST_METHOD=GET`, `REMOTE_ADDR`, `HTTP_USER_AGENT`, `SERVER_PROTOCOL=HTTP/2`.

```
+-----------------------------------------------------------------------------------+
|                        [CHÈN ẢNH CHỤP TERMINAL 5 TẠI ĐÂY]                         |
|       (Ảnh chụp danh sách các biến môi trường trả về từ chương trình env.cgi)     |
+-----------------------------------------------------------------------------------+
```
*Hình 5: Kết quả thực thi chương trình CGI env.cgi hiển thị các biến môi trường*

---

## 4. So sánh thực nghiệm giữa HTTP/1.0 và HTTP/1.1

**Mục tiêu:** Dùng lệnh `curl -I` để lấy tiêu đề phản hồi (HEAD request) của hai chuẩn HTTP và phân tích sự nâng cấp.

**Lệnh thực nghiệm:**
```bash
# 1. Gửi request theo chuẩn HTTP/1.0:
curl -I --http1.0 https://www.google.com

# 2. Gửi request theo chuẩn HTTP/1.1:
curl -I --http1.1 https://www.google.com
```

### Bảng đối chiếu đặc tính kỹ thuật giữa HTTP/1.0 và HTTP/1.1:
| Tiêu chí | HTTP/1.0 (RFC 1945) | HTTP/1.1 (RFC 2616 / RFC 7230) |
| :--- | :--- | :--- |
| **Quản lý kết nối TCP** | Ngắt kết nối sau mỗi request (Non-persistent). Mỗi ảnh/tập tin phải tạo 1 TCP handshake mới. | Mặc định duy trì kết nối (Persistent Connection / Keep-Alive), hỗ trợ Pipelining. |
| **Tiêu đề Host** | Tùy chọn (không bắt buộc), gây khó khăn cho việc chạy nhiều website trên 1 IP. | **Bắt buộc** phải có header `Host:`, hỗ trợ hoàn hảo Name-based Virtual Hosting. |
| **Cơ chế truyền tải** | Dựa trên `Content-Length` cố định trước. | Hỗ trợ `Transfer-Encoding: chunked` giúp truyền dữ liệu dạng luồng không cần biết trước dung lượng. |
| **Bộ nhớ đệm (Caching)** | Đơn giản: `If-Modified-Since`, `Expires`. | Mở rộng mạnh mẽ: `Cache-Control`, `ETag`, `If-None-Match`, `If-Match`. |

```
+-----------------------------------------------------------------------------------+
|                        [CHÈN ẢNH CHỤP TERMINAL 6 TẠI ĐÂY]                         |
|       (Ảnh chụp màn hình so sánh 2 khối kết quả của curl --http1.0 và --http1.1)  |
+-----------------------------------------------------------------------------------+
```
*Hình 6: So sánh tiêu đề phản hồi thực tế giữa giao thức HTTP/1.0 và HTTP/1.1*
