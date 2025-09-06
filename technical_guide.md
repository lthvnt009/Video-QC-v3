Hướng dẫn Kỹ thuật: Điều khiển qcli.exe và Phân tích Kết quả
Tác giả: Kiến Trúc Sư Lập Trình AI
Phiên bản: 2.0 (Bản Hoàn chỉnh)

1. Tổng quan Kiến trúc
Kiến trúc này hoạt động theo mô hình "hộp đen", nơi ứng dụng của chúng ta (Bảng điều khiển) sẽ điều khiển công cụ dòng lệnh qcli.exe như một tiến trình bên ngoài. Luồng hoạt động tổng thể như sau:

Xây dựng Lệnh: Bảng điều khiển thu thập các cài đặt từ người dùng và xây dựng một chuỗi lệnh hoàn chỉnh, được tối ưu hóa.

Thực thi & Giám sát: Khởi chạy qcli.exe trong một luồng nền (để không làm treo giao diện) và theo dõi luồng đầu ra (output stream) của nó theo thời gian thực.

Hiển thị Tiến trình: Phân tích luồng đầu ra để trích xuất thông tin tiến trình của cả hai giai đoạn và cập nhật giao diện.

Phân tích Kết quả: Sau khi qcli.exe hoàn tất, đọc file báo cáo XML mà nó đã tạo ra.

Áp dụng Logic Nghiệp vụ: Xử lý dữ liệu XML thô, áp dụng các quy tắc ưu tiên và thuật toán gom nhóm để xác định và định dạng các lỗi cuối cùng.

2. Bước 1: Xây dựng Lệnh Thực thi
Câu lệnh để gọi qcli.exe phải được xây dựng một cách "thông minh" để tối ưu hóa hiệu suất.

Cú pháp Lệnh Mẫu:
"đường/dẫn/tới/qcli.exe" -i "đường/dẫn/video.mp4" -o "đường/dẫn/file_report_tạm.xml" -y -s -f [chuỗi_bộ_lọc]

Giải thích các Tham số Bắt buộc:
Tham số

Ý nghĩa

Ví dụ

-i <file>

Input: Chỉ định file video đầu vào.

-i "C:\video.mp4"

-o <file>

Output: Chỉ định đường dẫn để lưu file báo cáo XML. Phải là một file tạm thời.

-o "C:\temp\report123.xml"

-y

Yes: Tự động đồng ý ghi đè nếu file báo cáo đã tồn tại.

-y

-s

Stats only: Yêu cầu qcli chỉ xuất ra dữ liệu thống kê (chế độ XML).

-s

-f <filters>

Filters: Chỉ định các bộ lọc cần chạy, nối với nhau bằng dấu +.

-f signalstats+cropdetect

Xây dựng Chuỗi Bộ lọc (-f)
Đây là bước tối ưu hóa quan trọng nhất. Dựa vào các checkbox trên giao diện, chương trình phải tự động xây dựng chuỗi này:

Nếu người dùng muốn tìm Frame Đen hoặc Frame Dư, chuỗi phải chứa signalstats.

Nếu người dùng muốn tìm Viền Đen, chuỗi phải chứa cropdetect.

Ví dụ: Nếu cả 3 lỗi đều được chọn, chuỗi sẽ là signalstats+cropdetect.

3. Bước 2 & 3: Thực thi và Hiển thị Tiến trình % (Quan trọng nhất)
Đây là phần phức tạp nhất do cơ chế đệm đầu ra (I/O Buffering) của hệ điều hành. Để vượt qua vấn đề này và đảm bảo tiến trình được cập nhật theo thời gian thực, phải tuân thủ nghiêm ngặt các kỹ thuật sau:

Kỹ thuật Đọc Đầu ra Chính xác:
Khởi chạy Tiến trình: Sử dụng một thư viện chuẩn như subprocess.Popen (Python) hoặc QProcess (C++/Qt).

Gộp Luồng: Khi khởi chạy, phải ra lệnh cho hệ điều hành gộp luồng báo lỗi (stderr) vào chung với luồng đầu ra chuẩn (stdout). Điều này đảm bảo chúng ta không bỏ sót bất kỳ thông báo nào.

Đọc Từng Ký tự (Không đọc theo dòng): Đây là chìa khóa. Thay vì dùng readline(), chương trình phải đọc đầu ra theo từng ký tự (read(1)) hoặc từng khối nhỏ. Việc này sẽ bypass hoàn toàn cơ chế đệm.

Kỹ thuật Phân tích Tiến trình:
Sử dụng Biểu thức Chính quy (Regex): Đây là phương pháp mạnh mẽ và đáng tin cậy nhất để tìm kiếm thông tin tiến trình. Biểu thức chính quy cần dùng là:

(\d+)\s+of\s+(\d+)\s*%

Logic Xử lý:

Khởi tạo một bộ đệm chuỗi (string buffer).

Trong một vòng lặp, đọc một ký tự từ luồng đầu ra của qcli.exe.

Nối ký tự đó vào bộ đệm.

Nếu ký tự là \n (xuống dòng) hoặc \r (về đầu dòng), hãy dùng biểu thức chính quy ở trên để tìm kiếm trong bộ đệm.

Nếu tìm thấy, trích xuất hai nhóm số đã bắt được (ví dụ: match.group(1) và match.group(2)), tính toán tỷ lệ %, và gửi thông điệp cập nhật lên giao diện.

Sau khi xử lý, xóa sạch bộ đệm.

Phân biệt Hai Giai đoạn Phân tích:

Khởi tạo một biến trạng thái, ví dụ current_phase = "Phân tích Video".

Trong quá trình đọc luồng đầu ra, nếu phát hiện chuỗi văn bản "generating QCTools report", hãy cập nhật current_phase = "Tạo Báo cáo".

Khi gửi thông điệp cập nhật tiến trình lên giao diện, hãy đính kèm cả giá trị của current_phase để hiển thị chi tiết (ví dụ: "Đang Tạo Báo cáo... 75%").

4. Bước 4 & 5: Phân tích Kết quả XML
Sau khi qcli.exe chạy xong và file report.xml đã được tạo, chương trình cần thực hiện các bước sau:

Thu thập Dữ liệu Thô:

Duyệt qua toàn bộ file XML một lần.

Với mỗi thẻ <frame>, đọc và lưu lại tất cả các dữ liệu cần thiết (YAVG, YDIF, crop_w, crop_h...) vào một danh sách các đối tượng/struct trong bộ nhớ. Bước này giúp tránh phải đọc lại file XML nhiều lần.

Áp dụng Logic Ưu tiên:

Chạy thuật toán phát hiện Frame Đen trước. Tất cả các frame bị xác định là "Frame Đen" sẽ được đánh dấu lại (ví dụ: thêm vào một set các số frame bị lỗi).

Khi chạy các thuật toán tiếp theo (Viền Đen, Frame Dư), chương trình phải bỏ qua, không xét đến những frame đã được đánh dấu là "Frame Đen".

Thuật toán Gom nhóm:

Frame Đen / Viền Đen:

Khởi tạo một "nhóm lỗi" rỗng.

Lặp qua danh sách dữ liệu thô. Nếu một frame thỏa mãn điều kiện lỗi:

Nếu "nhóm lỗi" đang rỗng, bắt đầu một nhóm mới.

Nếu frame này có cùng thuộc tính với nhóm hiện tại (ví dụ: cùng kích thước viền đen), chỉ cần cập nhật lại frame kết thúc của nhóm.

Nếu thuộc tính khác, đóng nhóm cũ lại, lưu vào danh sách kết quả, và bắt đầu một nhóm mới.

Frame Dư:

Lặp qua danh sách dữ liệu thô để tìm các điểm cắt cảnh (dựa trên YDIF và sceneThreshold).

Tính toán số frame giữa các điểm cắt cảnh.

Nếu số frame nhỏ hơn orphanThreshold, báo cáo đây là một lỗi "Frame Dư".

Định dạng Báo cáo:

Đặc biệt với lỗi "Viền Đen", báo cáo phải tính toán và hiển thị dải biến động (min/max) của kích thước viền cho từng cạnh, kèm theo cả số pixel và tỷ lệ phần trăm tương ứng.

Bằng cách tuân thủ nghiêm ngặt các hướng dẫn trên, bất kỳ lập trình viên nào cũng có thể xây dựng một "engine" điều khiển qcli.exe mạnh mẽ, chính xác và cung cấp phản hồi tiến trình theo thời gian thực.