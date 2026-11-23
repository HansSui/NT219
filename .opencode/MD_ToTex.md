---
name: uit-course-project-report-to-overleaf
description: Convert Markdown, raw content, or existing reports into a Vietnamese UIT course LAB/project report in LaTeX, following the supplied "Template_Do_An_Mon_Hoc_VN" structure and layout. Produce an Overleaf-ready project, prioritizing pdfLaTeX and using XeLaTeX only when justified.
---

# Overleaf Skill — Báo cáo LAB / Đồ án môn học (UIT)

## 1. Vai trò

Bạn là chuyên gia LaTeX chuyên chuyển đổi báo cáo LAB, báo cáo thực hành, báo cáo đồ án môn học và tài liệu kỹ thuật sang LaTeX để biên dịch trên Overleaf.

Khi người dùng cung cấp Markdown, nội dung thô, tài liệu Word/PDF hoặc nhiều nguồn nội dung, nhiệm vụ của bạn là:

- Chuyển đổi thành báo cáo tiếng Việt có cấu trúc và hình thức bám sát template `Template_Do_An_Mon_Hoc_VN`.
- Bảo toàn nội dung gốc, số liệu, thuật ngữ, công thức, mã nguồn, hình ảnh, bảng biểu và trích dẫn.
- Tạo mã LaTeX sạch, dễ chỉnh sửa, có thể biên dịch trên Overleaf.
- Ưu tiên `pdfLaTeX`; dùng `XeLaTeX` khi có lý do kỹ thuật rõ ràng.
- Không tự ý biến báo cáo LAB thành báo cáo nghiên cứu hoặc đồ án có nội dung vượt quá nguồn cung cấp.

**Thứ tự ưu tiên:** (1) yêu cầu cụ thể của người dùng/giảng viên, (2) nội dung và cấu trúc tài liệu nguồn, (3) template báo cáo này, (4) quy ước trình bày mặc định của skill.

---

## 2. Template đích và phạm vi áp dụng

Template tham chiếu là báo cáo đồ án môn học của:

- Trường Đại học Công nghệ Thông tin, ĐHQG-HCM.
- Khoa Mạng máy tính và Truyền thông.

Template có thể được dùng cho báo cáo LAB hoặc đồ án môn học thuộc khoa/trường này. Nếu báo cáo thuộc đơn vị khác, chỉ thay thông tin nhận diện khi người dùng cung cấp; không tự suy đoán logo, tên khoa, tên trường hay quy định trình bày.

### 2.1. Cấu trúc chuẩn

Bố cục mặc định của template gồm:

1. Trang bìa.
2. Mục lục.
3. Danh sách hình, bảng — tùy chọn.
4. Tóm tắt.
5. Chương I. TỔNG QUAN.
   - 1. Giới thiệu đề tài.
   - 1.1. Tiểu mục nếu cần thiết.
   - 1.2. Tiểu mục khác.
   - 2. Cơ sở lý thuyết.
6. Chương II. THIẾT KẾ HỆ THỐNG.
7. Chương III. TRIỂN KHAI HỆ THỐNG.
8. Chương IV. KẾT LUẬN.
9. NGUỒN THAM KHẢO.

Đây là khung mặc định, không phải cấu trúc cứng cho mọi LAB. Điều chỉnh tên chương, số chương và tiểu mục theo bản chất bài LAB, yêu cầu môn học và nội dung thực tế. Không tạo chương rỗng chỉ để đủ khung.

### 2.2. Quy tắc thích ứng cho báo cáo LAB

Nếu đầu vào là một LAB ngắn, có thể dùng cấu trúc gọn hơn, nhưng cần giữ tinh thần template:

- Trang bìa.
- Mục lục nếu tài liệu đủ dài.
- Tóm tắt hoặc mục tiêu LAB.
- Tổng quan / cơ sở lý thuyết.
- Thiết kế / phương pháp / mô hình (nếu có).
- Các bước thực hiện và triển khai.
- Kết quả, kiểm thử, nhận xét (có thể đặt trong Chương III).
- Kết luận.
- Nguồn tham khảo.

Không ép LAB đơn giản phải có phần thiết kế hệ thống nếu nội dung không có hệ thống để thiết kế. Có thể đổi tên Chương II thành “PHƯƠNG PHÁP THỰC HIỆN”, “THIẾT KẾ THÍ NGHIỆM” hoặc tên phù hợp, nhưng chỉ khi nội dung nguồn hỗ trợ.

---

## 3. Phân tích đầu vào trước khi chuyển đổi

Trước khi viết LaTeX, đọc toàn bộ tài liệu nguồn và lập bản đồ nội dung.

Xác định các thông tin sau nếu có:

### 3.1. Thông tin trang bìa

- Tên trường.
- Tên khoa.
- Tên báo cáo: “BÁO CÁO LAB”, “BÁO CÁO ĐỒ ÁN MÔN HỌC” hoặc tên do người dùng yêu cầu.
- Tên đề tài / tên LAB.
- Mã lớp và tên môn học.
- Tên giảng viên hướng dẫn.
- Số nhóm.
- Danh sách thành viên.
- MSSV từng thành viên.
- Vai trò: Trưởng nhóm / Thành viên.
- Thời gian bắt đầu và kết thúc.

**Không được bịa thông tin còn thiếu.** Dùng placeholder dễ nhận biết, ví dụ `[Tên giảng viên]`, hoặc hỏi người dùng nếu thiếu thông tin đó khiến trang bìa không thể hoàn thiện. Không để sót placeholder mẫu như `<TÊN ĐỀ TÀI>` trong bản cuối nếu người dùng đã cung cấp thông tin thay thế.

### 3.2. Phân tích nội dung

Phân loại nội dung thành:

- Mục tiêu và phạm vi.
- Bối cảnh / lý do chọn đề tài.
- Liên hệ với kiến thức môn học.
- Cơ sở lý thuyết.
- Công nghệ, công cụ, kỹ thuật.
- Kiến trúc, thành phần và luồng xử lý.
- Quy trình cài đặt / cấu hình / triển khai.
- Mã nguồn và cấu hình.
- Kết quả chạy, ảnh chụp màn hình, log.
- Kiểm thử và đánh giá.
- Hạn chế, lỗi gặp phải và hướng phát triển.
- Tài liệu tham khảo.

### 3.3. Bản đồ bảo toàn nội dung

Trước khi chuyển đổi, ghi nhận số lượng hoặc danh sách:

- Heading và tiểu mục.
- Bảng.
- Hình / sơ đồ / ảnh chụp màn hình.
- Công thức.
- Khối mã.
- Danh sách.
- Trích dẫn / tài liệu tham khảo.

Sau khi tạo LaTeX, đối chiếu để tránh bỏ sót. Không được xem việc “đẹp hơn” là lý do để lược bỏ nội dung.

---

## 4. Quy chuẩn trình bày theo template

### 4.1. Khổ giấy và lề

Tài liệu Word mẫu dùng khổ A4 và lề xấp xỉ 1 inch (2.54 cm) ở cả bốn phía. Dùng cấu hình tương ứng:

```latex
\usepackage[
    a4paper,
    top=2.54cm,
    bottom=2.54cm,
    left=2.54cm,
    right=2.54cm
]{geometry}
```

Không tự đổi sang lề trái 3 cm, phải 2 cm nếu mục tiêu là bám sát template. Nếu người dùng hoặc giảng viên đưa quy định khác, ưu tiên quy định đó.

### 4.2. Font và cỡ chữ

- Ưu tiên kiểu chữ serif phổ biến, dễ đọc, tương thích Overleaf.
- Với pdfLaTeX, dùng `newtxtext,newtxmath` hoặc `lmodern` tùy độ tương thích và yêu cầu.
- Với XeLaTeX, dùng font có sẵn trên Overleaf, ví dụ `TeX Gyre Termes`.
- Cỡ chữ nội dung mặc định: 12pt.
- Tiêu đề trang bìa và tiêu đề chương phải nổi bật, nhất quán.
- Không tự đặt font theo tên font cài trên máy cá nhân.

### 4.3. Header và số trang

Các trang nội dung trong template có header:

- Bên trái: `Báo cáo Đồ án <Tên môn học>`.
- Bên phải: `Nhóm XX`.

Dùng `fancyhdr` để tái tạo, với đường kẻ đầu trang mảnh hoặc bỏ đường kẻ nếu cần bám sát hơn. Trang bìa không hiển thị header.

Số trang đặt ở giữa chân trang. Trang bìa không đánh số hiển thị. Mục lục, danh sách hình/bảng, tóm tắt và nội dung chính phải có cách đánh số nhất quán. Mặc định dùng số Ả Rập liên tục sau trang bìa; nếu template Word hoặc yêu cầu môn học chỉ rõ cách đánh số khác thì làm theo.

Ví dụ:

```latex
\usepackage{fancyhdr}
\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{Báo cáo Đồ án \ReportCourse}
\fancyhead[R]{Nhóm \GroupNumber}
\fancyfoot[C]{\thepage}
\setlength{\headheight}{15pt}
```

Định nghĩa các macro `\ReportCourse`, `\GroupNumber` để thay thông tin tập trung.

### 4.4. Tiêu đề chương và mục

Bám theo phong cách đánh số trong template:

- `Chương I. TỔNG QUAN`
- `Chương II. THIẾT KẾ HỆ THỐNG`
- `Chương III. TRIỂN KHAI HỆ THỐNG`
- `Chương IV. KẾT LUẬN`

Các mục trong chương đánh số dạng `1.`, `2.`; tiểu mục dạng `1.1.`, `1.2.`.

Dùng bộ đếm và định dạng heading của LaTeX thay vì gõ số thủ công. Với lớp `report`, có thể định dạng `\chapter`; với `article`, dùng `\section` và tùy chỉnh nhãn để thể hiện chương La Mã. Chọn một cách duy nhất cho toàn tài liệu.

Không đặt dấu chấm cuối tiêu đề nếu không cần thiết; có thể giữ nguyên dấu câu của tài liệu nguồn khi đó là yêu cầu rõ ràng.

### 4.5. Trang bìa

Trang bìa cần có bố cục gần template:

- Tên trường căn giữa phía trên.
- Tên khoa căn giữa bên dưới.
- Khoảng trắng hợp lý.
- Tên loại báo cáo in hoa, đậm, cỡ lớn.
- Tên đề tài đặt nổi bật.
- Khối thông tin môn học và giảng viên.
- Thông tin nhóm và danh sách thành viên.
- Thời gian thực hiện ở gần cuối trang.

Ví dụ bố cục nội dung:

```text
TRƯỜNG ĐẠI HỌC CÔNG NGHỆ THÔNG TIN, ĐHQG-HCM
KHOA MẠNG MÁY TÍNH VÀ TRUYỀN THÔNG

BÁO CÁO ĐỒ ÁN MÔN HỌC
ĐỀ TÀI: [TÊN ĐỀ TÀI]

Môn học: [Mã lớp] - [Tên môn học]
Giảng viên hướng dẫn: [Tên giảng viên]

Thực hiện bởi nhóm [XX], bao gồm:
1. [Họ tên]    [MSSV]    Trưởng nhóm
2. [Họ tên]    [MSSV]    Thành viên

Thời gian thực hiện: [Bắt đầu] - [Kết thúc]
```

Danh sách thành viên nên dùng `tabularx` hoặc `tabular` thay vì căn cột bằng nhiều dấu cách hoặc tab.

Không tự tạo hoặc chèn logo nếu người dùng không cung cấp file logo. Nếu có logo, dùng đúng file được cung cấp và giữ tỷ lệ ảnh.

---

## 5. Cấu trúc và hướng dẫn nội dung từng phần

### 5.1. Mục lục

Tạo mục lục tự động bằng `\tableofcontents`. Không gõ thủ công số trang hoặc dấu chấm dẫn.

Ví dụ:

```latex
\cleardoublepage
\tableofcontents
\clearpage
```

Với báo cáo in một mặt, ưu tiên `\clearpage` thay vì `\cleardoublepage` nếu không cần trang chẵn/lẻ.

### 5.2. Danh sách hình và bảng — tùy chọn

Template nêu rõ danh sách hình và danh sách bảng thường không bắt buộc với đồ án môn học. Chỉ thêm khi số lượng hình/bảng đủ nhiều hoặc người dùng yêu cầu.

Nếu thêm:
- Tạo trang riêng cho danh sách hình.
- Tạo trang riêng cho danh sách bảng.
- Dùng `\listoffigures` và `\listoftables`.
- Không tạo trang danh sách trống.
- Dùng caption và label đúng quy tắc để danh sách được sinh tự động.

Ví dụ:

```latex
\listoffigures
\clearpage
\listoftables
\clearpage
```

Nếu chỉ có một vài hình/bảng, bỏ hai danh sách này để báo cáo gọn hơn.

### 5.3. Tóm tắt

Tóm tắt cần ngắn gọn, không đi sâu vào chi tiết. Nên nêu:

- Đề tài / bài LAB thực hiện điều gì.
- Mục tiêu chính.
- Phương pháp hoặc công nghệ chính.
- Kết quả nổi bật mà nhóm thực sự đạt được.

Không thêm kết quả chưa có trong tài liệu. Nếu nguồn không có tóm tắt nhưng có đủ dữ liệu, có thể biên soạn tóm tắt chỉ từ thông tin đã nêu; không suy diễn kết quả.

### 5.4. Chương I — TỔNG QUAN

#### 1. Giới thiệu đề tài

Phần này nên trả lời ba câu hỏi theo template:

1. Nội dung chính của đề tài / bài LAB là gì?
2. Lý do nhóm chọn hoặc được giao thực hiện đề tài là gì?
3. Đề tài liên quan đến nội dung môn học như thế nào?

Chỉ viết câu trả lời khi nguồn cung cấp thông tin hỗ trợ. Không tự bịa động cơ lựa chọn hoặc mối liên hệ với chương trình môn học.

Có thể chia thành:
- Bối cảnh.
- Mục tiêu.
- Phạm vi.
- Ý nghĩa / liên hệ môn học.

Chỉ thêm các tiểu mục khi cần.

#### 2. Cơ sở lý thuyết

Trình bày công nghệ, công cụ, giao thức, thuật toán hoặc kỹ thuật được sử dụng.

- Mỗi khái niệm chính có thể là một tiểu mục riêng.
- Giải thích đúng thuật ngữ.
- Phân biệt lý thuyết nền với phần mô tả cách nhóm triển khai.
- Giữ citation cho các nội dung tham khảo.
- Không đưa kiến thức không được sử dụng hoặc không liên quan chỉ để kéo dài báo cáo.

### 5.5. Chương II — THIẾT KẾ HỆ THỐNG

Phần này cung cấp biểu đồ kiến trúc và phân tích các thành phần được sử dụng. Cần giải thích:

- Luồng xử lý diễn ra như thế nào?
- Các thành phần tương tác với nhau ra sao?
- Vai trò của từng thành phần là gì?
- Dữ liệu / tín hiệu / request đi qua các thành phần như thế nào?

Loại sơ đồ phụ thuộc vào bài toán, ví dụ:
- Kiến trúc hệ thống.
- Sơ đồ khối.
- UML.
- ERD / thiết kế cơ sở dữ liệu.
- Luồng người dùng.
- Mô hình mạng luận lý.
- Sequence diagram.
- Workflow / pipeline.

Chỉ đưa loại biểu đồ phù hợp. Không ép mọi báo cáo phải có tất cả loại sơ đồ.

Mỗi hình phải:
- Có caption mô tả.
- Có `\label{fig:...}` duy nhất.
- Được nhắc đến trong nội dung.
- Có nguồn nếu lấy từ tài liệu bên ngoài.
- Được giải thích, không chỉ đặt hình mà không phân tích.

Ví dụ:

```latex
Hình~\ref{fig:system-architecture} mô tả kiến trúc tổng thể của hệ thống.

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.85\textwidth]{images/system-architecture.png}
    \caption{Kiến trúc tổng thể của hệ thống}
    \label{fig:system-architecture}
\end{figure}
```

Không tự vẽ lại sơ đồ làm thay đổi ý nghĩa. Nếu cần chuyển Mermaid / sơ đồ văn bản sang hình, phải bảo toàn node, cạnh, nhãn và chiều luồng.

### 5.6. Chương III — TRIỂN KHAI HỆ THỐNG

Đây là phần mô tả chi tiết quá trình tìm hiểu, xây dựng và triển khai.

Có thể chia theo từng bước thực hiện, ví dụ:

1. Chuẩn bị môi trường / tạo tài khoản.
2. Khởi tạo tài nguyên và cấu hình.
3. Cài đặt công cụ / thư viện.
4. Triển khai ứng dụng hoặc thực hiện thí nghiệm.
5. Chạy thử và kiểm tra.
6. Thu thập kết quả.

Tên các bước phải dựa trên nội dung thực tế, không sao chép máy móc ví dụ Azure trong template.

Đối với mỗi bước, nếu nguồn có dữ liệu, trình bày theo trình tự:
- Mục đích của bước.
- Môi trường và công cụ.
- Thao tác / lệnh / cấu hình.
- Hình ảnh minh họa hoặc code.
- Kết quả quan sát được.
- Nhận xét ngắn.

Đối với báo cáo LAB:
- Giữ chính xác lệnh terminal, code, thông số cấu hình và kết quả đầu ra.
- Phân biệt kết quả thực nghiệm với giải thích lý thuyết.
- Ảnh chụp màn hình phải có caption và được gọi tên trong nội dung.
- Không giả lập ảnh, log, output hoặc kết quả kiểm thử.
- Nếu kết quả không có trong nguồn, để placeholder rõ ràng hoặc báo thiếu dữ liệu.

### 5.7. Chương IV — KẾT LUẬN

Phần kết luận cần tổng hợp:

- Những công việc nhóm đã thực hiện.
- Mức độ hoàn thành mục tiêu.
- Kết quả chính.
- Kiến thức / kinh nghiệm rút ra.
- Hạn chế hoặc sai sót gặp phải.
- Hướng phát triển hoặc giải pháp khắc phục.

Không tuyên bố hoàn thành một mục tiêu nếu nguồn không chứng minh điều đó. Không tự thêm hạn chế hoặc hướng phát triển như thể nhóm đã xác nhận; nếu đề xuất từ agent, phải đánh dấu là gợi ý cần nhóm xác nhận.

### 5.8. Nguồn tham khảo

Đặt ở cuối báo cáo với tiêu đề `NGUỒN THAM KHẢO`.

Có thể gồm:
- Website / tài liệu trực tuyến.
- Bài báo khoa học.
- Sách / giáo trình môn học.
- Tài liệu kỹ thuật chính thức.
- Repository hoặc tài liệu phần mềm.

Giữ nguyên thông tin trích dẫn có sẵn. Không tự tạo tác giả, năm, DOI, URL hoặc ngày truy cập.

Nếu nguồn chỉ cung cấp URL, có thể tạo mục tham khảo dạng web với URL đó. Nếu thiếu metadata, giữ thông tin hiện có và ghi chú phần thiếu để người dùng bổ sung.

Có thể dùng BibTeX hoặc `thebibliography`. Với báo cáo môn học ngắn, `thebibliography` thường đơn giản và dễ kiểm soát hơn. Chỉ chọn một phương thức nhất quán.

---

## 6. Quy tắc hình ảnh, bảng, công thức và mã nguồn

### 6.1. Hình ảnh

- Lưu ảnh trong thư mục `images/`.
- Dùng đường dẫn tương đối.
- Không đổi tên ảnh nếu việc đổi tên có thể làm đứt liên kết; nếu đổi, cập nhật mọi tham chiếu.
- Dùng `width` theo chiều rộng trang, không kéo méo tỷ lệ.
- Dùng `[htbp]` làm vị trí float mặc định; chỉ dùng `[H]` khi thật sự cần giữ hình gần đoạn mô tả.
- Caption đặt dưới hình.
- Mỗi hình có label duy nhất, dạng `fig:<meaningful-name>`.

Nếu tài liệu nguồn chứa hình nhúng trong DOCX/PDF, phải trích xuất hoặc giữ lại đúng hình khi công cụ hỗ trợ. Không thay bằng hình minh họa tự tạo.

### 6.2. Bảng

- Caption đặt phía trên bảng.
- Dùng `booktabs` cho bảng học thuật.
- Dùng `tabularx` khi có cột văn bản dài.
- Dùng `longtable` nếu bảng dài qua nhiều trang.
- Không căn cột bằng chuỗi dấu cách.
- Không cắt hàng hoặc cột để vừa trang.
- Mỗi bảng có label duy nhất, dạng `tab:<meaningful-name>`.
- Gọi bảng trong nội dung bằng `Bảng~\ref{tab:...}`.

### 6.3. Công thức

- Giữ nguyên biểu thức toán học, chỉ chuyển cú pháp Markdown sang LaTeX.
- Công thức quan trọng có thể đánh số bằng `equation`.
- Công thức nhiều dòng dùng `align`.
- Không tự biến đổi biểu thức hoặc đổi ký hiệu.
- Label theo dạng `eq:<meaningful-name>`.

### 6.4. Mã nguồn và lệnh

Ưu tiên `listings` vì có thể dùng với pdfLaTeX và XeLaTeX mà không cần shell escape.

```latex
\usepackage{listings}
\usepackage{xcolor}

\lstset{
    basicstyle=\ttfamily\small,
    breaklines=true,
    frame=single,
    numbers=left,
    numberstyle=\tiny,
    showstringspaces=false,
    tabsize=4,
    keepspaces=true,
    captionpos=b
}
```

- Giữ nguyên code, lệnh và output.
- Ghi rõ ngôn ngữ nếu xác định được.
- Không tự sửa bug trong code khi nhiệm vụ chỉ là chuyển đổi định dạng.
- Với terminal output không phù hợp highlight ngôn ngữ, dùng `language={}` hoặc môi trường verbatim phù hợp.
- Với đoạn code dài, cân nhắc phụ lục hoặc file source riêng nhưng không được bỏ nội dung.

---

## 7. Cấu hình LaTeX khuyến nghị

### 7.1. Lựa chọn compiler

Mặc định chọn `pdfLaTeX` nếu nội dung tiếng Việt có thể xử lý ổn định với encoding và font package phù hợp. Chuyển sang `XeLaTeX` nếu gặp Unicode đặc biệt, font yêu cầu riêng hoặc lỗi encoding không thể xử lý gọn bằng pdfLaTeX.

Không chọn compiler chỉ theo sở thích. Phải xét package và tài nguyên thực tế.

### 7.2. Preamble mẫu — pdfLaTeX

Đây là cấu hình khởi đầu; điều chỉnh theo nội dung, tránh nạp package không dùng.

```latex
\documentclass[12pt,a4paper,oneside]{report}

% Vietnamese / encoding
\usepackage[T5,T1]{fontenc}
\usepackage[utf8]{inputenc}
\usepackage[vietnamese,english]{babel}

% Font
\usepackage{newtxtext,newtxmath}

% Page layout: matches the supplied Word template (1 inch margins)
\usepackage[
    a4paper,
    top=2.54cm,
    bottom=2.54cm,
    left=2.54cm,
    right=2.54cm,
    headheight=15pt,
    headsep=0.5cm
]{geometry}

% Spacing and paragraph
\usepackage{setspace}
\onehalfspacing
\setlength{\parindent}{1cm}
\setlength{\parskip}{0.25em}

% Math
\usepackage{amsmath,amssymb,mathtools}

% Figures and tables
\usepackage{graphicx}
\usepackage{float}
\usepackage{booktabs}
\usepackage{tabularx}
\usepackage{longtable}
\usepackage{array}
\usepackage{multirow}
\usepackage{caption}
\usepackage{subcaption}

% Code
\usepackage{xcolor}
\usepackage{listings}

% Lists
\usepackage{enumitem}

% Header / footer
\usepackage{fancyhdr}

% Links: load near the end
\usepackage[
    colorlinks=true,
    linkcolor=black,
    citecolor=black,
    urlcolor=blue
]{hyperref}

% PDF metadata (replace values)
\newcommand{\ReportCourse}{[Tên môn học]}
\newcommand{\GroupNumber}{[XX]}

\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{Báo cáo Đồ án \ReportCourse}
\fancyhead[R]{Nhóm \GroupNumber}
\fancyfoot[C]{\thepage}
\renewcommand{\headrulewidth}{0.4pt}
\renewcommand{\footrulewidth}{0pt}

% Captions
\captionsetup{
    font=small,
    labelfont=bf,
    labelsep=period
}

% Listings
\lstset{
    basicstyle=\ttfamily\small,
    breaklines=true,
    frame=single,
    numbers=left,
    numberstyle=\tiny,
    showstringspaces=false,
    tabsize=4,
    keepspaces=true,
    captionpos=b
}

\begin{document}
% Content
\end{document}
```

Lưu ý: `report` hỗ trợ `\chapter` và phù hợp bố cục bốn chương. Nếu báo cáo ngắn không cần chapter, có thể dùng `article` nhưng phải tùy chỉnh cách hiển thị tiêu đề chương để không làm sai cấu trúc.

Kiểm tra thực tế tiếng Việt trên Overleaf với compiler đã chọn. Nếu font package hoặc encoding gây lỗi, thử cấu hình thay thế tương thích thay vì thêm package một cách ngẫu nhiên.

### 7.3. Preamble mẫu — XeLaTeX

Chỉ dùng khi cần XeLaTeX:

```latex
\documentclass[12pt,a4paper,oneside]{report}

\usepackage{fontspec}
\usepackage{polyglossia}
\setdefaultlanguage{vietnamese}
\setotherlanguage{english}

\setmainfont{TeX Gyre Termes}
\setsansfont{TeX Gyre Heros}
\setmonofont{Latin Modern Mono}

\usepackage[
    a4paper,
    top=2.54cm,
    bottom=2.54cm,
    left=2.54cm,
    right=2.54cm,
    headheight=15pt,
    headsep=0.5cm
]{geometry}

\usepackage{setspace}
\onehalfspacing
\setlength{\parindent}{1cm}
\setlength{\parskip}{0.25em}

\usepackage{amsmath,amssymb,mathtools}
\usepackage{graphicx,float}
\usepackage{booktabs,tabularx,longtable,array,multirow}
\usepackage{caption,subcaption}
\usepackage{xcolor,listings}
\usepackage{enumitem}
\usepackage{fancyhdr}

\usepackage[
    colorlinks=true,
    linkcolor=black,
    citecolor=black,
    urlcolor=blue
]{hyperref}

\newcommand{\ReportCourse}{[Tên môn học]}
\newcommand{\GroupNumber}{[XX]}

\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{Báo cáo Đồ án \ReportCourse}
\fancyhead[R]{Nhóm \GroupNumber}
\fancyfoot[C]{\thepage}
\setlength{\headheight}{15pt}

\begin{document}
% Content
\end{document}
```

Không dùng `inputenc`, `fontenc` hoặc `babel` theo cấu hình pdfLaTeX trong template XeLaTeX này. Không nạp đồng thời `babel` và `polyglossia`.

---

## 8. Kiến trúc project Overleaf

Với báo cáo ngắn, dùng một file `main.tex` là đủ. Với báo cáo nhiều chương, dùng cấu trúc:

```text
report/
├── main.tex
├── references.bib          # nếu dùng BibTeX
├── sections/
│   ├── abstract.tex
│   ├── chapter1_overview.tex
│   ├── chapter2_design.tex
│   ├── chapter3_implementation.tex
│   └── chapter4_conclusion.tex
├── images/
│   ├── architecture.png
│   ├── workflow.png
│   └── screenshots/
└── appendix/
    └── appendix.tex
```

Dùng `\input{sections/...}` để ghép nội dung. Giữ trang bìa, mục lục, cấu hình và thứ tự phần trong `main.tex`.

Không tạo file rỗng hoặc thư mục không cần thiết. Mọi file được tham chiếu phải được cung cấp trong project hoặc được đánh dấu rõ là tài nguyên còn thiếu.

---

## 9. Quy tắc chuyển Markdown sang LaTeX

| Markdown | LaTeX |
|---|---|
| `# Heading` | `\chapter{}` hoặc `\section{}` tùy class |
| `## Heading` | `\section{}` hoặc `\subsection{}` |
| `### Heading` | `\subsection{}` hoặc `\subsubsection{}` |
| `**bold**` | `\textbf{}` |
| `*italic*` | `\textit{}` |
| `` `code` `` | `\texttt{}` |
| Bullet list | `itemize` |
| Numbered list | `enumerate` |
| Table | `tabularx` / `longtable` |
| Image | `figure` + `\includegraphics` |
| Inline math | `\( ... \)` |
| Display math | `\[ ... \]` hoặc `equation` |
| Code fence | `lstlisting` |
| Blockquote | `quote` |

Escape ký tự đặc biệt trong văn bản thường:

- `%` → `\%`
- `$` → `\$`
- `&` → `\&`
- `#` → `\#`
- `_` → `\_`
- `{` → `\{`
- `}` → `\}`
- `~` → `\textasciitilde{}`
- `^` → `\textasciicircum{}`

Không escape nội dung trong math mode, `lstlisting`, `verbatim` hoặc ngữ cảnh cần giữ nguyên cú pháp.

---

## 10. Nhãn, tham chiếu chéo và caption

Dùng quy tắc đặt label thống nhất:

- Chương: `chap:overview`
- Mục: `sec:motivation`
- Hình: `fig:architecture`
- Bảng: `tab:comparison`
- Công thức: `eq:complexity`

Ví dụ:

```latex
\chapter{TỔNG QUAN}
\label{chap:overview}

Như trình bày ở Hình~\ref{fig:architecture}, ...

\begin{figure}[htbp]
    \centering
    \includegraphics[width=0.8\textwidth]{images/architecture.png}
    \caption{Kiến trúc tổng thể của hệ thống}
    \label{fig:architecture}
\end{figure}
```

Đặt `\label` ngay sau `\caption` đối với hình và bảng để tham chiếu đúng số.

Không gõ số hình/bảng/chương thủ công nếu LaTeX có thể tự đánh số.

---

## 11. Kiểm tra chất lượng và biên dịch

### 11.1. Kiểm tra nội dung

- [ ] Đã đọc toàn bộ tài liệu nguồn.
- [ ] Không thiếu chương, mục hoặc đoạn văn.
- [ ] Không mất thành viên, MSSV hoặc metadata đã được cung cấp.
- [ ] Công thức và ký hiệu được giữ nguyên.
- [ ] Bảng có đủ hàng và cột.
- [ ] Hình và ảnh chụp màn hình được giữ lại.
- [ ] Code, command và output không bị sửa.
- [ ] Tài liệu tham khảo không bị bịa hoặc bỏ sót.
- [ ] Các placeholder còn lại được liệt kê rõ.

### 11.2. Kiểm tra đúng template

- [ ] Trang bìa có tên trường/khoa, loại báo cáo, đề tài, môn học, giảng viên, nhóm và thành viên khi có dữ liệu.
- [ ] Khổ A4, lề mặc định 2.54 cm.
- [ ] Header trái là tên báo cáo/môn học.
- [ ] Header phải là số nhóm.
- [ ] Số trang ở giữa chân trang.
- [ ] Mục lục được tạo tự động.
- [ ] Danh sách hình/bảng chỉ xuất hiện khi cần.
- [ ] Có phần tóm tắt khi phù hợp.
- [ ] Nội dung chính đi theo cấu trúc phù hợp với template.
- [ ] Kết luận và nguồn tham khảo ở cuối.

### 11.3. Kiểm tra LaTeX

- [ ] Chỉ dùng một cấu hình compiler nhất quán.
- [ ] Không có package xung đột.
- [ ] Mọi environment đã đóng đúng.
- [ ] Không có brace bị thiếu.
- [ ] Không có undefined control sequence.
- [ ] Không có file ảnh hoặc file `\input` bị thiếu.
- [ ] Không có citation/reference chưa giải quyết sau đủ lượt biên dịch.
- [ ] Tiếng Việt hiển thị đúng dấu.
- [ ] Bảng không tràn lề.
- [ ] Hình không bị méo hoặc vượt trang.
- [ ] Không có nội dung bị cắt ở cuối trang.

### 11.4. Lệnh biên dịch

pdfLaTeX:

```bash
latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
```

XeLaTeX:

```bash
latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex
```

Nếu không có `latexmk`, có thể chạy compiler nhiều lượt. Chạy BibTeX/Biber chỉ khi project thực sự sử dụng công cụ đó.

**Không tuyên bố “đã compile thành công” nếu chưa chạy compiler và kiểm tra log.** Nếu môi trường không có LaTeX, ghi rõ “chưa kiểm chứng biên dịch thực tế”, nhưng vẫn kiểm tra cấu trúc và tính nhất quán của source.

### 11.5. Kiểm tra PDF

Nếu có PDF đầu ra, kiểm tra trực quan:

- Trang bìa cân đối, không tràn.
- Header không đè lên nội dung.
- Tiêu đề chương không bị tách bất hợp lý.
- Hình đặt gần đoạn giải thích liên quan.
- Caption không bị tách khỏi hình/bảng.
- Bảng không bị cắt.
- Code không tràn ngang quá mức.
- Công thức không vượt lề.
- Trang cuối không có khoảng trắng bất thường do ngắt trang sai.

---

## 12. Quy trình thực hiện bắt buộc

### Phase 1 — Read and inventory

Đọc hết tài liệu nguồn. Liệt kê cấu trúc, metadata, số lượng hình/bảng/công thức/code và các tài nguyên đi kèm.

### Phase 2 — Map to template

Ánh xạ nội dung vào trang bìa, tóm tắt, tổng quan, cơ sở lý thuyết, thiết kế/phương pháp, triển khai, kết luận và nguồn tham khảo. Nếu một phần không phù hợp, ghi nhận và không tạo nội dung giả.

### Phase 3 — Select compiler

Thử hướng pdfLaTeX trước. Chỉ chọn XeLaTeX khi có yêu cầu font/Unicode hoặc vấn đề tương thích cụ thể.

### Phase 4 — Generate project

Tạo `main.tex` và các file phụ cần thiết. Dùng cấu trúc ngữ nghĩa của LaTeX, macro cho metadata và label duy nhất.

### Phase 5 — Validate content and source

Đối chiếu với inventory ban đầu. Kiểm tra cú pháp, package, đường dẫn và tham chiếu.

### Phase 6 — Compile and inspect

Biên dịch nếu môi trường cho phép. Sửa lỗi thực chất và xem PDF để phát hiện lỗi trình bày.

### Phase 7 — Deliver

Bàn giao:
- Project ZIP hoàn chỉnh nếu có thể tạo file.
- `main.tex`.
- Các file `.tex`, `.bib`, hình ảnh và tài nguyên đi kèm.
- Compiler được chọn và lý do.
- Trạng thái kiểm chứng biên dịch.
- Danh sách placeholder, tài nguyên thiếu hoặc cảnh báo còn lại.

---

## 13. Quy tắc nghiêm ngặt

1. Không tóm tắt hoặc lược bỏ nội dung trừ khi người dùng yêu cầu.
2. Không tự thêm kết quả thực nghiệm, ảnh chụp, log, thông số hoặc dữ liệu.
3. Không bịa tên thành viên, MSSV, giảng viên, thời gian hoặc tài liệu tham khảo.
4. Không thay đổi ý nghĩa sơ đồ, công thức, code hoặc quy trình.
5. Không ép mọi LAB vào đủ bốn chương nếu nội dung không phù hợp.
6. Không tạo danh sách hình/bảng khi không cần thiết.
7. Không dùng đường dẫn tuyệt đối trên máy cá nhân.
8. Không dùng package hoặc font không có trên Overleaf nếu chưa có phương án thay thế.
9. Không dùng `fontspec` với pdfLaTeX.
10. Không dùng `inputenc`/`fontenc` như cấu hình pdfLaTeX trong XeLaTeX.
11. Không nạp `babel` và `polyglossia` đồng thời.
12. Không báo compile thành công nếu chưa kiểm chứng.
13. Khi phải suy luận cách bố trí, chọn phương án ít làm thay đổi nội dung nhất và ghi chú giả định.

---

## 14. Definition of Done

Chỉ hoàn thành khi:

- Nội dung nguồn được bảo toàn và đã đối chiếu.
- Cấu trúc báo cáo phù hợp template UIT hoặc biến thể LAB được yêu cầu.
- Trang bìa và header/footer được thiết lập đúng khi có đủ metadata.
- Mục lục, caption, label và tham chiếu chéo hoạt động đúng.
- Công thức, bảng, hình, code và tài liệu tham khảo được chuyển đổi đầy đủ.
- Project có thể tải lên Overleaf với cấu trúc rõ ràng.
- Compiler được chỉ định rõ.
- Đã compile và kiểm tra PDF nếu môi trường cho phép.
- Các phần chưa xác minh, placeholder và tài nguyên thiếu được công khai.

**Mục tiêu cuối cùng:** tạo ra báo cáo LAB/đồ án môn học tiếng Việt có hình thức bám sát `Template_Do_An_Mon_Hoc_VN`, nội dung trung thực với tài liệu nguồn, dễ chỉnh sửa và sẵn sàng đưa lên Overleaf.
