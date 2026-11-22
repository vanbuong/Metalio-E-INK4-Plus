#include "assets/lang_app_store.h"
#include "assets/lang_workbuddy.h"
#include "assets/lang_reader.h"
#include "assets/lang_config.h"
#include "assets/lang_screen.h"
#include "assets/lang_pixel.h"
#include "assets/lang_log.h"
#include "assets/lang_haptic.h"
#include "assets/lang_network.h"
#include "assets/lang_charging.h"
#include "assets/lang_wallpaper.h"
#include "assets/lang_bluetooth.h"

#include "settings.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Lang"

// X(name, vi, en)
#define LANG_STRING_LIST(X)                                                                                    \
    X(STORE_PREVIOUS_PAGE, "Trước", "Previous") \
    X(STORE_NEXT_PAGE, "Sau", "Next") \
    X(STORE_PAGE_RESULTS, "Trang này không có ứng dụng đã cài", "Not installed on this page") \
    X(STORE_READING_INSTALLED, "Đang đọc ứng dụng đã cài…", "Reading installed apps…") \
    X(STORE_FETCHING, "Đang tải danh sách ứng dụng…", "Fetching store apps…") \
    X(STORE_FETCHING_DETAIL, "Đang tải chi tiết ứng dụng…", "Fetching app details…") \
    X(STORE_REFRESHED, "Đã cập nhật danh sách ứng dụng", "App list updated") \
    X(STORE_OFFLINE_INSTALLED, "Không kết nối được Kho ứng dụng. Ứng dụng đã cài vẫn dùng được.", "Cannot connect to App Store. Installed apps are still available.") \
    X(STORE_NO_INSTALLED, "Chưa cài ứng dụng nào", "No installed apps") \
    X(STORE_EMPTY, "Không có ứng dụng", "No apps available") \
    X(STORE_QUEUED, "Đang chờ", "Queued") \
    X(STORE_CANCELLING, "Đang hủy", "Cancelling") \
    X(STORE_CANCELLED, "Đã hủy cài đặt", "Installation cancelled") \
    X(STORE_VERIFYING, "Đang kiểm tra…", "Verifying…") \
    X(STORE_INSTALLING, "Đang cài đặt…", "Installing…") \
    X(STORE_REMOVING, "Đang gỡ ứng dụng…", "Uninstalling app…") \
    X(STORE_INSTALLED_DONE, "Đã cài ứng dụng", "App installed") \
    X(STORE_UPDATED, "Đã cập nhật ứng dụng", "App updated") \
    X(STORE_REMOVED, "Đã gỡ ứng dụng", "App uninstalled") \
    X(STORE_FILTER, "Lọc", "Filter") \
    X(STORE_ALL, "Tất cả", "All") \
    X(STORE_ALL_APPS, "Tất cả ứng dụng", "All apps") \
    X(STORE_DOWNLOADING, "Đang tải", "Downloading") \
    X(STORE_UNINSTALLED, "Chưa cài", "Not installed") \
    X(STORE_INSTALL_STATE, "Cài đặt", "Installation") \
    X(STORE_DETAILS, "Chi tiết ứng dụng", "App details") \
    X(STORE_VERSION, "Phiên bản", "Version") \
    X(STORE_SIZE, "Dung lượng", "App size") \
    X(STORE_CATEGORY, "Thể loại", "Category") \
    X(STORE_LANGUAGE, "Ngôn ngữ", "Languages") \
    X(STORE_LANGUAGE_ZH, "Tiếng Trung giản thể", "Simplified Chinese") \
    X(STORE_LANGUAGE_EN, "Tiếng Anh", "English") \
    X(STORE_DESCRIPTION, "Mô tả", "Description") \
    X(STORE_RELEASE_NOTES, "Ghi chú phát hành", "Release notes") \
    X(STORE_PROVIDER, "Nhà cung cấp", "Provider") \
    X(STORE_LAST_UPDATED, "Cập nhật lần cuối", "Last updated") \
    X(STORE_NOT_PROVIDED, "Không có", "Not provided") \
    X(STORE_LOCATION, "Lưu trữ", "Storage") \
    X(STORE_CHECKING, "Đang kiểm tra", "Checking") \
    X(STORE_CANCEL, "Hủy", "Cancel") \
    X(STORE_NO_MATCH, "Không có ứng dụng phù hợp", "No matching apps") \
    X(STORE_TITLE, "Kho ứng dụng", "App Store") \
    X(STORE_INSTALLED, "Đã cài", "Installed") \
    X(STORE_INSTALL, "Cài đặt", "Install") \
    X(STORE_UPDATE, "Cập nhật", "Update") \
    X(STORE_CHECK_UPDATE, "Kiểm tra cập nhật", "Check for updates") \
    X(STORE_UPDATE_CONFIRM_FMT, "Có phiên bản mới: %s\nCập nhật ứng dụng này?", "New version available: %s\nUpdate this app?") \
    X(STORE_CONFIRM, "Xác nhận", "Confirm") \
    X(STORE_RUN, "Mở", "Open") \
    X(STORE_REMOVE, "Gỡ cài đặt", "Uninstall") \
    X(STORE_LOADING, "Đang tải ứng dụng…", "Loading apps…") \
    X(STORE_BUSY, "Đang xử lý. Vui lòng chờ…", "Working. Please wait…") \
    X(STORE_DOWNLOAD_BUSY, "Đang tải xuống. Vui lòng thử lại sau.", "Download in progress. Please try again later.") \
    X(STORE_CURRENT, "Đã là bản mới nhất", "Already up to date") \
    X(STORE_REMOVE_CONFIRM, "Gỡ ứng dụng này? Tệp cài đặt trên máy sẽ bị xóa. Bạn có thể cài lại từ Kho.", "Uninstall this app? Local installation files will be removed. You can install it again from the Store.") \
    X(STORE_SD_ERROR, "Thẻ SD không dùng được hoặc ghi lỗi. Kiểm tra thẻ và chế độ USB.", "SD card unavailable or write failed. Check the card and USB mode.") \
    X(STORE_FORBIDDEN, "Không dùng được liên kết thiết bị. Kiểm tra liên kết thiết bị.", "Device binding unavailable. Check device binding.") \
    X(STORE_UNAVAILABLE, "Phiên bản ứng dụng không có sẵn. Hãy làm mới danh sách.", "App version unavailable. Refresh the list.") \
    X(STORE_CATALOG_EXPIRED, "Danh sách ứng dụng đã hết hạn. Hãy làm mới danh sách.", "App list expired. Refresh the list.") \
    X(STORE_WITHDRAWN, "Phiên bản này đã bị thu hồi. Hãy làm mới danh sách.", "This version was withdrawn. Refresh the list.") \
    X(STORE_PACKAGE_CHANGED, "Thông tin tải xuống đã thay đổi. Hãy cài lại.", "Download information changed. Install again.") \
    X(STORE_RANGE_ERROR, "Tiến trình tải không hợp lệ. Hãy cài lại.", "Download progress invalid. Install again.") \
    X(STORE_UNSUPPORTED, "Thiết bị hoặc firmware không tương thích. Kiểm tra firmware.", "Device or firmware incompatible. Check firmware.") \
    X(STORE_SERVICE_BUSY, "Dịch vụ đang bận. Thử lại sau.", "Service busy. Try again later.") \
    X(STORE_NETWORK_ERROR, "Không kết nối được Kho ứng dụng. Kiểm tra mạng.", "Cannot connect to App Store. Check the network.") \
    X(STORE_VERIFY_ERROR, "Kiểm tra tệp ứng dụng thất bại. Hãy tải lại.", "App file verification failed. Download it again.") \
    X(STORE_MEMORY_ERROR, "Không đủ bộ nhớ. Đóng tính năng khác rồi thử lại.", "Not enough memory. Close other features and try again.") \
    X(STORE_INCOMPATIBLE, "Ứng dụng không tương thích với firmware này nên không chạy được. Cập nhật firmware rồi thử lại.", "App is incompatible with this firmware and cannot start. Update firmware and try again.") \
    X(STORE_RUN_ERROR, "Ứng dụng không khởi động được. Kiểm tra tương thích ứng dụng và firmware.", "App failed to start. Check app and firmware compatibility.") \
    X(STORE_INVALID, "Danh mục ứng dụng không hợp lệ hoặc phiên bản API không tương thích.", "Invalid app catalog or incompatible API version.") \
    X(STORE_LIMIT, "Hỗ trợ tối đa 32 ứng dụng. Hãy gỡ bớt ứng dụng không dùng.", "Up to 32 apps are supported. Uninstall unused apps first.") \
    X(STORE_SERVER_ERROR, "Địa chỉ kho không hợp lệ. Kiểm tra cài đặt dịch vụ.", "Invalid store address. Check service settings.") \
    X(YUNXIN_BINDING_OPEN_AI, "Mở NetEase AI và quét mã để liên kết thiết bị.", "Open NetEase AI and scan the code to link your device.") \
    X(YUNXIN_BINDING_CHECKING, "Đang kiểm tra liên kết thiết bị. Vui lòng chờ...", "Checking device link. Please wait...") \
    X(YUNXIN_BINDING_SCAN, "Quét bằng mini program để liên kết thiết bị.\nTính năng này dùng được sau khi liên kết xong.", "Scan with the mini program to link your device.\nThis feature is available once linking is complete.") \
    X(YUNXIN_BINDING_RETRY, "Chưa xác nhận được liên kết thiết bị. Đang thử lại...\nGiữ thiết bị kết nối Wi-Fi.", "Unable to confirm the device link. Retrying...\nKeep your device connected to Wi-Fi.") \
    X(BOOK_FONT_DECREASE, "Chữ nhỏ hơn", "Smaller text") \
    X(BOOK_FONT_INCREASE, "Chữ lớn hơn", "Larger text") \
    X(BOOK_TURN_INTERVAL, "Khoảng thời gian", "Interval") \
    X(BOOK_DEFAULT, "Mặc định", "Default") \
    X(BOOK_DECREASE, "Giảm", "Decrease") \
    X(BOOK_INCREASE, "Tăng", "Increase") \
    X(BOOK_LETTER_GAP, "Giãn chữ", "Letter spacing") \
    X(BOOK_INDENT, "Thụt đầu dòng", "First-line indent") \
    X(BOOK_AA_STRENGTH, "Cường độ", "Strength") \
    X(BOOK_AA_LOW, "Thấp", "Low") \
    X(BOOK_AA_MEDIUM, "Vừa", "Medium") \
    X(BOOK_AA_HIGH, "Cao", "High") \
    X(BOOK_FOOTER_LAYOUT, "Bố cục thông tin", "Information layout") \
    X(BOOK_FOOTER_BOTTOM, "Tất cả ở dưới", "All at bottom") \
    X(BOOK_UNDERLINE_POSITION, "Vị trí đường kẻ", "Line position") \
    X(BOOK_UNDERLINE_UP, "Lên 2px", "Up 2px") \
    X(BOOK_UNDERLINE_CENTER, "Đặt lại", "Reset") \
    X(BOOK_UNDERLINE_DOWN, "Xuống 2px", "Down 2px") \
    X(BOOK_INDENT_ONE, "1 ký tự", "1 char") \
    X(BOOK_INDENT_TWO, "2 ký tự", "2 chars") \
    X(BOOK_INDENT_THREE, "3 ký tự", "3 chars") \
    X(SETTINGS_TEST_FRONTLIGHT_START, "Chạm để bật", "Tap to turn on") \
    X(DEVICE_ACTIVATE_FIRST, "Hãy liên kết thiết bị trong ứng dụng CloudZao trước", "Please link your device in the CloudZao app first") \
    X(XIAOZHI_ACTIVATE_FIRST, "Hãy liên kết thiết bị tại xiaozhi.me trước", "Please link your device at xiaozhi.me first") \
    X(STORAGE_IO_ERROR, "Lỗi đọc/ghi thẻ SD", "SD card read/write error") \
    X(AUDIO_SERVICE_NOT_READY, "Dịch vụ âm thanh chưa sẵn sàng. Thử lại sau giây lát.", "Audio service is not ready. Try again shortly.") \
    X(AUDIO_CALLBACK_UNAVAILABLE, "Không ghi âm được. Hãy mở lại ứng dụng.", "Recording is unavailable. Reopen the app.") \
    X(AUDIO_RECORDING_BUSY, "Bản ghi trước chưa xong. Thử lại sau giây lát.", "The previous recording is still finishing. Try again shortly.") \
    X(AUDIO_MIC_BUSY, "Micro đang được dùng. Dừng bản ghi kia rồi thử lại.", "The microphone is in use. Stop the other recording and retry.") \
    X(AUDIO_RECORDING_MEMORY, "Không đủ bộ nhớ để ghi âm. Thử lại sau giây lát.", "Not enough memory to record. Try again shortly.") \
    X(AUDIO_PROCESSOR_FAILED, "Khởi tạo ghi âm thất bại. Hãy mở lại ứng dụng.", "Recording initialization failed. Reopen the app.") \
    X(TTF_SIZE_FAILED_FMT, "Không đặt được cỡ chữ %d", "Cannot set font size %d") \
    X(TTF_SD_IO_FAILED, "Đọc/ghi thẻ SD thất bại. Kiểm tra thẻ.", "SD card read/write failed. Check the card.") \
    X(TTF_NO_GLYPHS_FMT, "Không có ký tự nào ở cỡ %d", "No glyphs available at size %d") \
    X(TTF_WRITE_FAILED, "Không ghi được phông. Kiểm tra thẻ SD.", "Cannot write font. Check the SD card.") \
    X(TTF_VERIFY_FAILED, "Kiểm tra phông thất bại. Hãy chuyển đổi lại.", "Font verification failed. Convert again.") \
    X(TTF_TEMPLATE_INVALID, "Phông mẫu không hợp lệ. Hãy nhập lại.", "Invalid template font. Import it again.") \
    X(TTF_ENGINE_INIT_FAILED, "Khởi tạo bộ dựng phông thất bại", "Font engine initialization failed") \
    X(TTF_SOURCE_OPEN_FAILED, "Không mở được phông nguồn. Kiểm tra tệp.", "Cannot open source font. Check the file.") \
    X(TTF_JOB_INVALID, "Tác vụ chuyển đổi không hợp lệ. Hãy chuyển đổi lại.", "Invalid conversion job. Convert again.") \
    X(TTF_START_FAILED, "Không bắt đầu chuyển đổi được. Thử lại sau giây lát.", "Cannot start conversion. Try again shortly.") \
    X(TTF_CANCELLED, "Đã hủy chuyển đổi", "Conversion cancelled") \
    X(TTF_GLYPH_MEMORY, "Không đủ bộ nhớ cho ký tự. Hãy giảm cỡ chữ.", "Not enough memory for glyphs. Reduce the font size.") \
    X(TTF_CANCELLED_OR_NO_SD, "Đã hủy chuyển đổi hoặc không có thẻ SD", "Conversion cancelled or SD card unavailable") \
    X(ASSISTANT_PHOTO_FAILED, "Không chụp được. Kiểm tra camera rồi thử lại.", "Cannot capture. Check the camera and retry.") \
    X(ASSISTANT_PHOTO_BUSY, "Đang chụp, ghi âm hoặc tải lên. Thử lại sau giây lát.", "Capture, recording or upload in progress. Retry shortly.") \
    X(ASSISTANT_PHOTO_CLOSE, "Hãy đóng ảnh xem trước trước.", "Close the image preview first.") \
    X(ASSISTANT_PHOTO_UPLOADING, "Đang tải ảnh lên…", "Uploading image…") \
    X(ASSISTANT_PHOTO_ANALYSIS_FAILED, "Phân tích ảnh thất bại. Đã giữ ảnh. Hãy thử lại.", "Image analysis failed. Photo kept. Retry.") \
    X(ASSISTANT_PHOTO_DISABLED, "Dịch vụ ảnh đang tắt. Đã giữ ảnh.", "Image service is disabled. Photo kept.") \
    X(ASSISTANT_PHOTO_SENT, "Đã gửi câu hỏi kèm ảnh.", "Image question sent.") \
    X(ASSISTANT_PHOTO_QUESTION_FAILED, "Chưa gửi được câu hỏi. Đã giữ ảnh. Hãy thử lại.", "Question not sent. Photo kept. Retry.") \
    X(ASSISTANT_PHOTO_HISTORY_FAILED, "Đã tải ảnh lên nhưng chưa thêm vào trò chuyện. Hãy thử lại.", "Image uploaded but not added to chat. Retry.") \
    X(ASSISTANT_PHOTO_UPLOAD_FAILED, "Tải lên thất bại. Đã giữ ảnh. Hãy thử lại.", "Upload failed. Photo kept. Retry.") \
    X(ASSISTANT_RELEASE_SEND, "Thả tay để gửi", "Release to send") \
    X(ASSISTANT_SEND, "Gửi", "Send") \
    X(ASSISTANT_USER_PREFIX, "Bạn: ", "User: ") \
    X(ASSISTANT_CONNECTING_HOLD, "Đang kết nối dịch vụ giọng nói. Tiếp tục giữ.", "Connecting to voice service. Keep holding.") \
    X(BOOK_IMPORT_TTF, "Nhập TTF", "Import TTF") \
    X(BOOK_ORIENT_AUTO, "Tự động", "Auto") \
    X(BOOK_ORIENT_TITLE, "Hướng màn hình", "Orientation") \
    X(BOOK_WRITING_HORIZONTAL, "Ngang", "Horizontal") \
    X(BOOK_WRITING_VERTICAL, "Dọc", "Vertical") \
    X(BOOK_WRITING_TITLE, "Bố cục chữ", "Text layout") \
    X(BOOK_TEXT_DIRECTION, "Hướng chữ", "Text direction") \
    X(BOOK_TEXT_LEFT_TO_RIGHT, "Trái sang phải", "Left to right") \
    X(BOOK_TEXT_RIGHT_TO_LEFT, "Phải sang trái", "Right to left") \
    X(BOOK_TURN_SLIDE, "Trượt", "Slide") \
    X(BOOK_TURN_PUSH, "Quét", "Wipe") \
    X(BOOK_TURN_TITLE, "Cài đặt lật trang", "Page turn settings") \
    X(BOOK_SHAKE_TURN, "Lắc để lật", "Shake to turn") \
    X(BOOK_SHAKE_LEVEL, "Độ nhạy", "Sensitivity") \
    X(BOOK_AUTO_TURN, "Tự lật trang", "Auto turn") \
    X(BOOK_PRESET_SMALL, "Nhỏ", "Small") \
    X(BOOK_PRESET_LARGE, "Lớn", "Large") \
    X(BOOK_PRESET_XLARGE, "Rất lớn", "X-large") \
    X(BOOK_MARGIN_TITLE, "Lề", "Margins") \
    X(BOOK_SPACING_TITLE, "Giãn dòng", "Line spacing") \
    X(BOOK_PARA_GAP_TITLE, "Cách đoạn", "Paragraph gap") \
    X(BOOK_BACKGROUND_GRID, "Lưới", "Grid") \
    X(BOOK_BACKGROUND_DOTS, "Chấm", "Dots") \
    X(BOOK_BACKGROUND_TITLE, "Nền", "Background") \
    X(BOOK_CUSTOM, "Tùy chỉnh", "Custom") \
    X(BOOK_FONT_LOADING, "Đang tải phông...", "Loading fonts...") \
    X(BOOK_FONT_REOPEN, "Tải phông thất bại. Hãy mở lại thẻ Phông.", "Font loading failed. Reopen the Fonts tab.") \
    X(BOOK_FONT_IMPORT_EMPTY, "Chưa có phông. Hãy nhập phông TTF.", "No fonts. Import a TTF font.") \
    X(BOOK_BOOKMARKS, "Dấu trang", "Bookmarks") \
    X(BOOK_LAYOUT_WAIT, "Đang dàn trang. Thử lại sau giây lát.", "Laying out text. Try again shortly.") \
    X(COMMON_MEMORY_RETRY, "Không đủ bộ nhớ. Thử lại sau giây lát.", "Not enough memory. Try again shortly.") \
    X(BOOK_BOOKMARKS_EMPTY, "Chưa có dấu trang", "No bookmarks") \
    X(BOOK_DURATION_HM_FMT, "%u giờ %u phút", "%uh %um") \
    X(BOOK_READING_PROGRESS, "Tiến độ đọc", "Reading progress") \
    X(BOOK_CHAPTER_PREV, "Chương trước", "Prev chapter") \
    X(BOOK_CHAPTER_NEXT, "Chương sau", "Next chapter") \
    X(BOOK_PAGE_PREV, "Trang trước", "Previous page") \
    X(BOOK_PAGE_NEXT, "Trang sau", "Next page") \
    X(BOOK_PAGE_FMT, "Trang %d", "Page %d") \
    X(BOOK_COMIC_SETTING_SAVED, "Đã lưu. Cài đặt này không áp dụng cho truyện tranh.", "Saved. This setting does not apply to comics.") \
    X(BOOK_FONT_SELECT, "Chọn phông", "Choose font") \
    X(BOOK_READING_SETTINGS, "Cài đặt đọc", "Reading settings") \
    X(BOOK_SELECTED_ITEMS_FMT, "Đã chọn %u", "Selected %u") \
    X(BOOK_TAP_MENU_REQUIRED, "Cần giữ ít nhất một vùng chạm cho Menu.", "Keep at least one tap zone assigned to Menu.") \
    X(TTF_FINISHING, "Chuyển đổi sắp xong. Thử lại sau giây lát.", "Conversion is finishing. Try again shortly.") \
    X(TTF_SOURCE, "Phông nguồn", "Source font") \
    X(TTF_OUTPUT, "Đầu ra", "Output") \
    X(TTF_SUCCESS, "Đã chuyển đổi phông và thêm vào danh sách phông.", "Font converted and added to the font list.") \
    X(TTF_CONVERTING, "Đang chuyển đổi", "Converting") \
    X(TTF_FAILED, "Chuyển đổi thất bại", "Conversion failed") \
    X(TTF_EMPTY, "Không tìm thấy phông TTF/OTF\nĐặt phông vào metalio/e-ink/fonts_ttf trên thẻ SD", "No TTF/OTF fonts found\nPlace fonts in metalio/e-ink/fonts_ttf on the SD card") \
    X(TTF_SIZE_RANGE, "Cỡ chữ (20–80px)", "Font size (20–80px)") \
    X(TTF_OVERWRITE_HINT, "Tệp .ef cùng tên sẽ bị thay thế. Rời trang này hoặc vào chế độ chờ sẽ hủy chuyển đổi. Giữ thẻ SD trong máy.\nDùng trực tiếp sẽ làm lật trang chậm hơn.", "Existing .ef files with the same name will be replaced. Leaving this page or entering standby cancels conversion. Keep the SD card inserted.\nApplying directly slows page turns.") \
    X(TTF_START, "Chuyển đổi", "Convert") \
    X(TTF_APPLY, "Dùng trực tiếp", "Apply directly") \
    X(TTF_APPLY_FAILED, "Không tải được phông. Kiểm tra tệp hoặc giảm cỡ chữ.", "Cannot load font. Check the file or reduce the size.") \
    X(TTF_OUTPUT_NAME_TOO_LONG, "Tên đầu ra quá dài hoặc không hợp lệ. Hãy rút ngắn tên tệp nguồn rồi thử lại.", "Output name too long or invalid. Shorten the source filename and retry.") \
    X(TTF_GENERATING, "Đang tạo phông", "Generating font") \
    X(TTF_PROGRESS_HINT, "Phông đã chuyển đổi sẽ xuất hiện trong danh sách phông. Rời trang này hoặc vào chế độ chờ sẽ hủy chuyển đổi. Giữ thẻ SD trong máy.", "The converted font will appear in the font list. Leaving this page or entering standby cancels conversion. Keep the SD card inserted.") \
    X(TTF_FAILED_FMT, "Chuyển đổi thất bại: %.60s", "Conversion failed: %.60s") \
    X(TTF_BACK_TO_LIST, "Về danh sách nhập", "Back to imports") \
    X(BOOK_VIEW_COVER, "Bìa sách", "Covers") \
    X(BOOK_VIEW_LIST, "Danh sách", "List") \
    X(BOOK_WEEK_READING, "Tuần này", "This week") \
    X(BOOK_MINUTES_FMT, "%.1f phút", "%.1f min") \
    X(BOOK_WEEK_MON, "T2", "M") \
    X(BOOK_WEEK_TUE, "T3", "T") \
    X(BOOK_WEEK_WED, "T4", "W") \
    X(BOOK_WEEK_THU, "T5", "T") \
    X(BOOK_WEEK_FRI, "T6", "F") \
    X(BOOK_WEEK_SAT, "T7", "S") \
    X(BOOK_WEEK_SUN, "CN", "S") \
    X(BOOK_WEEK_DURATION, "Tuần này", "This week") \
    X(BOOK_TOTAL_DURATION, "Tổng thời gian đọc", "Total reading time") \
    X(TRANSFER_SD_CAPACITY_FAILED, "Không đọc được dung lượng thẻ SD", "Cannot read SD card capacity") \
    X(TRANSFER_RECOVERY_FAILED, "Không đọc được thư mục hoặc khôi phục tệp tải lên", "Cannot read directory or recover uploads") \
    X(TRANSFER_DIRECTORY_MISSING, "Không tìm thấy thư mục", "Directory not found") \
    X(TRANSFER_DIRECTORY_FAILED, "Không đọc được thư mục. Kiểm tra thẻ SD rồi làm mới.", "Cannot read directory. Check the SD card and refresh.") \
    X(TRANSFER_DOWNLOAD_BUSY, "Chờ tải lên xong rồi mới tải xuống.", "Wait for the upload to finish before downloading.") \
    X(TRANSFER_READ_FAILED, "Không đọc được tệp", "Cannot read file") \
    X(TRANSFER_SD_FULL, "Thẻ SD đã đầy. Tệp gốc được giữ nguyên.", "SD card is full. The original file is preserved.") \
    X(TRANSFER_MEMORY, "Không đủ bộ nhớ. Đã dừng tải lên; tệp gốc được giữ nguyên. Thử lại sau giây lát.", "Not enough memory. Upload stopped; the original file is preserved. Try again shortly.") \
    X(STORAGE_RESTART_REQUIRED, "Lỗi thẻ SD. Khởi động lại thiết bị rồi thử lại.", "SD card error. Restart the device and try again.") \
    X(TRANSFER_WRITE_FAILED, "Ghi thẻ SD thất bại. Tệp gốc được giữ nguyên. Kiểm tra thẻ rồi thử lại.", "SD card write failed. The original file is preserved. Check the card and retry.") \
    X(TRANSFER_DISCONNECTED, "Mất kết nối. Đã xóa tệp chưa hoàn tất; tệp gốc được giữ nguyên.", "Disconnected. Uncommitted files were removed; the original file is preserved.") \
    X(TRANSFER_UPLOAD_BUSY, "Đang có lượt tải lên. Thử lại sau giây lát.", "An upload is in progress. Try again shortly.") \
    X(TRANSFER_INVALID_ID, "Mã tải lên không hợp lệ", "Invalid upload ID") \
    X(TRANSFER_ALREADY_HANDLED, "Lượt tải lên đã được xử lý. Làm mới thư mục để xem kết quả.", "Upload already processed. Refresh the directory to check the result.") \
    X(TRANSFER_ENCODING_UNSUPPORTED, "Kiểu mã hóa tải lên không được hỗ trợ. Làm mới trang rồi thử lại.", "Unsupported upload encoding. Refresh the page and retry.") \
    X(TRANSFER_INVALID_FILE, "Tên, đường dẫn hoặc kích thước tệp không hợp lệ", "Invalid file name, path or size") \
    X(TRANSFER_INVALID_SIZE, "Kích thước tệp không hợp lệ", "Invalid file size") \
    X(TRANSFER_SD_CAPACITY_UNAVAILABLE, "Không đọc được dung lượng thẻ SD", "Cannot read SD card capacity") \
    X(TRANSFER_EXISTS_CHOOSE, "Đã có mục cùng tên. Hãy chọn cách xử lý.", "An item with this name already exists. Choose how to proceed.") \
    X(TRANSFER_TARGET_CHECK_FAILED, "Không kiểm tra được tệp đích", "Cannot check the destination file") \
    X(TRANSFER_TEMP_FAILED, "Không tạo được tệp tạm. Tệp gốc được giữ nguyên.", "Cannot create temporary file. The original file is preserved.") \
    X(TRANSFER_INCOMPLETE, "Tải lên chưa hoàn tất. Tệp gốc được giữ nguyên.", "Incomplete upload. The original file is preserved.") \
    X(TRANSFER_CANCELLED, "Đã hủy tải lên. Tệp gốc được giữ nguyên.", "Upload cancelled. The original file is preserved.") \
    X(TRANSFER_TARGET_CHANGED, "Mục đích đã thay đổi. Hãy tải lên lại.", "The destination item has changed. Upload again.") \
    X(TRANSFER_TARGET_CHECK_PRESERVED, "Không kiểm tra được tệp đích. Tệp gốc được giữ nguyên.", "Cannot check the destination file. The original file is preserved.") \
    X(TRANSFER_BACKUP_FAILED, "Không sao lưu được tệp gốc. Đã hủy thay thế.", "Cannot back up the original file. Replacement cancelled.") \
    X(TRANSFER_COMMIT_FAILED, "Lưu thất bại. Tệp gốc được giữ nguyên.", "Save failed. The original file is preserved.") \
    X(TRANSFER_RESTORE_FAILED, "Lưu thất bại. Bản sao lưu gốc được giữ nguyên. Khôi phục thẻ SD, làm mới rồi thử lại.", "Save failed. The original backup is preserved. Restore the SD card, refresh and retry.") \
    X(TRANSFER_INVALID_STREAM, "Dữ liệu nén hoặc độ dài không hợp lệ. Tệp gốc được giữ nguyên.", "Invalid compressed data or length. The original file is preserved.") \
    X(TRANSFER_UPLOAD_MISSING, "Không có lượt tải lên cho kết nối này", "No upload found for this connection") \
    X(TRANSFER_INVALID_COMMAND, "Lệnh tải lên không hợp lệ", "Invalid upload command") \
    X(TRANSFER_PATH_BUSY, "Một lượt tải lên đang dùng đường dẫn này. Chờ nó hoàn tất.", "An upload is using this path. Wait for it to finish.") \
    X(TRANSFER_CREATE_FAILED, "Tạo thất bại", "Creation failed") \
    X(TRANSFER_DELETE_ROOT_INVALID, "Đường dẫn không hợp lệ hoặc không được xóa thư mục gốc", "Invalid path or root directory deletion is not allowed") \
    X(TRANSFER_DIRECTORY_BUSY, "Thư mục này đang có lượt tải lên. Chờ nó hoàn tất.", "This directory contains an active upload. Wait for it to finish.") \
    X(TRANSFER_DELETE_PARTIAL, "Một số mục không xóa được. Tệp có thể đang được dùng, đường dẫn quá dài hoặc thẻ SD lỗi. Mục đã xóa không khôi phục được. Làm mới để kiểm tra.", "Some items could not be deleted. Files may be in use, paths too long, or the SD card faulty. Deleted items cannot be restored. Refresh to check.") \
    X(TRANSFER_PATH_UNAVAILABLE, "Đường dẫn quá dài hoặc thư mục không dùng được", "Path is too long or directory is unavailable") \
    X(TRANSFER_EXISTS, "Đã có mục cùng tên", "An item with this name already exists") \
    X(TRANSFER_ITEM_CHECK_FAILED, "Không kiểm tra được mục đích", "Cannot check the destination item") \
    X(TRANSFER_RENAME_FAILED, "Đổi tên thất bại", "Rename failed") \
    X(TRANSFER_MOVE_SELF, "Không thể di chuyển vào chính nó hoặc thư mục con", "Cannot move into itself or a subdirectory") \
    X(TRANSFER_DEST_UNAVAILABLE, "Đường dẫn quá dài hoặc đích không dùng được", "Path is too long or destination is unavailable") \
    X(TRANSFER_DEST_EXISTS, "Ở đích đã có mục cùng tên", "An item with this name already exists at the destination") \
    X(TRANSFER_MOVE_FAILED, "Di chuyển thất bại", "Move failed") \
    X(TRANSFER_HOTSPOT_STOPPING, "Điểm phát đang tắt", "Hotspot is shutting down") \
    X(TRANSFER_REQUEST_BUSY, "Dịch vụ tệp đang bận. Thử lại sau giây lát.", "File service is busy. Try again shortly.") \
    X(CLOUD_WIFI_PREPARING, "Đang chuẩn bị truyền qua Wi-Fi...", "Preparing Wi-Fi transfer...") \
    X(CLOUD_TAB_WIFI, "Wi-Fi", "Wi-Fi") \
    X(COMMON_RESTART, "Khởi động lại", "Restart") \
    X(NETWORK_SCANNING_SHORT, "Đang quét", "Scanning") \
    X(NETWORK_SAVED_COUNT_FMT, "Đã lưu (%d)", "Saved (%d)") \
    X(NETWORK_UNSAVED_COUNT_FMT, "Chưa lưu (%d)", "Unsaved (%d)") \
    X(NETWORK_NO_SAVED_NEARBY, "Không có mạng đã lưu ở gần", "No saved networks nearby") \
    X(NETWORK_NO_UNSAVED_NEARBY, "Không có mạng chưa lưu ở gần", "No unsaved networks nearby") \
    X(NETWORK_TRANSFER_READY, "Truyền qua Wi-Fi đã sẵn sàng", "Wi-Fi transfer is ready") \
    X(NETWORK_TRANSFER_OPEN_HINT, "Kết nối điện thoại hoặc máy tính vào cùng mạng Wi-Fi.\nQuét mã hoặc mở địa chỉ bên dưới trong trình duyệt.", "Connect your phone or computer to the same Wi-Fi network.\nScan the code or open the address below in a browser.") \
    X(NETWORK_TRANSFER_STOP_HINT, "Đổi chế độ truyền hoặc rời trang này sẽ dừng truyền.", "Switching transfer mode or leaving this page stops the transfer.") \
    X(NETWORK_BACK_TO_LIST, "Về danh sách", "Back to list") \
    X(NETWORK_CURRENT_SSID_FMT, "Mạng: %s", "Network: %s") \
    X(QUICK_ENABLED, "Bật", "On") \
    X(QUICK_DISABLED, "Tắt", "Off") \
    X(QUICK_SWITCHING, "Đang chuyển", "Switching") \
    X(QUICK_FAILED, "Thử lại", "Retry") \
    X(QUICK_CONNECTED, "Đã kết nối", "Connected") \
    X(QUICK_CONNECTING, "Đang kết nối", "Connecting") \
    X(QUICK_DISCONNECTED, "Ngoại tuyến", "Offline") \
    X(SETTINGS_DISPLAY_GRAY_HINT, "Sau khi xóa màn hình sẽ hiện 16 mức xám gốc: 00 là đen; 15 là trắng.", "After clearing, 16 native gray levels are displayed: 00 is black; 15 is white.") \
    X(SETTINGS_DISPLAY_BUSY, "Màn hình đang bận. Thử lại sau giây lát.", "Display is busy. Try again shortly.") \
    X(SETTINGS_TEST_DISPLAY, "Kiểm tra màn hình", "Display test") \
    X(SETTINGS_DISPLAY_START_HINT, "Chạm 16 mức xám để bắt đầu.\nBên dưới sẽ hiện 16 dải từ đen đến trắng, đánh số 00–15.", "Tap 16 gray levels to start.\n16 bars from black to white should appear below, numbered 00–15.") \
    X(SETTINGS_DISPLAY_GRAY, "16 mức xám", "16 gray levels") \
    X(SETTINGS_DISPLAY_DITHER, "So sánh dither", "Dither comparison") \
    X(SETTINGS_DISPLAY_DITHER_HINT, "So sánh dither: điểm đen và trắng mô phỏng mức xám.", "Dither comparison: black and white pixels simulate gray levels.") \
    X(SETTINGS_DISPLAY_RETEST_HINT, "Chạm để lặp lại. NEXT để thử lại; PREV để quay lại.", "Tap to repeat. NEXT retests; PREV goes back.") \
    X(WB_STATUS_QUERYING, "(Đang kiểm tra)", "(Checking)") \
    X(WB_STATUS_FAILED, "(Kiểm tra thất bại)", "(Check failed)") \
    X(WB_STATUS_ONLINE, "(Trực tuyến)", "(Online)") \
    X(WB_STATUS_OFFLINE, "(Ngoại tuyến)", "(Offline)") \
    X(WB_STATUS_UNKNOWN, "(Không rõ)", "(Unknown)") \
    X(WB_AUTH_CHECKING, "Đang kiểm tra ủy quyền WorkBuddy...", "Checking WorkBuddy authorization...") \
    X(WB_SYNC_MESSAGES, "Đồng bộ tin nhắn", "Sync messages") \
    X(WB_REFRESHING, "Đang làm mới", "Refreshing") \
    X(WB_AUTH_SAVED_CHECKING, "Đã lưu ủy quyền. Đang kiểm tra kết nối máy tính...", "Authorization saved. Checking the computer connection...") \
    X(WB_CONNECTED, "Đã kết nối máy tính. Sẵn sàng sử dụng.", "Computer connected. Ready to use.") \
    X(WB_BOUND_OFFLINE, "Thiết bị đã liên kết; máy tính đang ngoại tuyến.\nMở WorkBuddy trên máy tính bằng tài khoản đã liên kết, rồi chọn Đồng bộ tin nhắn trong menu.", "Device linked; computer is offline.\nOpen WorkBuddy on your computer with the linked account, then choose Sync messages from the menu.") \
    X(WB_BOUND_CONNECT_FAILED, "Thiết bị đã liên kết. Không kết nối được máy tính:\n", "Device linked. Cannot connect to the computer:\n") \
    X(WB_RELINK_HINT, "\n\nXử lý sự cố rồi chọn Liên kết lại trong menu góc trên bên phải.", "\n\nResolve the issue, then choose Relink from the top-right menu.") \
    X(WB_SYNC_RETRY_HINT, "\n\nChọn Đồng bộ tin nhắn trong menu để thử lại. Không cần quét lại.", "\n\nChoose Sync messages from the menu to retry. No need to scan again.") \
    X(WB_QR_GENERATING, "Đang tạo mã ủy quyền mới...", "Generating a new authorization code...") \
    X(WB_SCAN_FRIEND, "Nhờ bạn của bạn quét và ủy quyền bằng tài khoản của họ.", "Ask your friend to scan and authorize with their own account.") \
    X(WB_SCAN_AUTHORIZE, "Quét bằng điện thoại để đăng nhập và ủy quyền.", "Scan with your phone to sign in and authorize.") \
    X(WB_AUTH_INCOMPLETE, "Ủy quyền chưa xong. Làm mới mã rồi thử lại.", "Authorization incomplete. Refresh the code and retry.") \
    X(WB_QR_EXPIRED, "Mã đã hết hạn. Chạm Làm mới.", "Code expired. Tap Refresh.") \
    X(WB_AUTH_RECONNECTING, "Kết nối bị gián đoạn. Đang kiểm tra lại ủy quyền...", "Connection interrupted. Checking authorization again...") \
    X(WB_AUTHORIZE_PHONE, "Hoàn tất ủy quyền Tencent WorkBuddy trên điện thoại.", "Complete Tencent WorkBuddy authorization on your phone.") \
    X(WB_WAIT_CLOSE, "Đang chờ kết nối trước đóng lại...", "Waiting for the previous connection to close...") \
    X(WB_WAIT_MEMORY, "Không đủ bộ nhớ. Đang chờ tài nguyên...", "Not enough memory. Waiting for resources...") \
    X(WB_RELINK_PREPARING, "Đang chuẩn bị liên kết lại...", "Preparing to relink...") \
    X(WB_LINK_CHECKING, "Đang kiểm tra liên kết thiết bị. Vui lòng chờ...", "Checking device link. Please wait...") \
    X(WB_REPLY_INTERRUPTED, "Đã nhận yêu cầu nhưng việc lấy câu trả lời bị gián đoạn:\n", "Request accepted, but reply retrieval was interrupted:\n") \
    X(WB_TASK_CONTINUES_HINT, "\n\nTác vụ có thể vẫn đang chạy. Chọn Đồng bộ tin nhắn để kiểm tra. Tránh gửi lại cùng yêu cầu.", "\n\nThe task may still be running. Choose Sync messages to check. Avoid sending the same request again.") \
    X(WB_REPLY_TIMEOUT, "Đã nhận yêu cầu nhưng chưa có câu trả lời sau ba phút. Tác vụ có thể vẫn đang chạy. Hãy chọn Đồng bộ tin nhắn sau để kiểm tra. Tránh gửi lại cùng yêu cầu.", "Request accepted, but no reply received within three minutes. The task may still be running. Choose Sync messages later to check. Avoid sending the same request again.") \
    X(WB_RECORDING_CANCELLED, "Đã hủy ghi âm", "Recording cancelled") \
    X(AUDIO_MODULE_INIT_FAILED, "Khởi tạo mô-đun âm thanh thất bại. Kiểm tra kết nối rồi khởi động lại thiết bị.", "Audio module initialization failed. Check the connection and restart the device.") \
    X(AUDIO_MODULE_NOT_READY, "Mô-đun âm thanh chưa sẵn sàng. Thử lại sau giây lát.", "Audio module is not ready. Try again shortly.") \
    X(WB_RECORDING_TASK_BUSY, "Ghi âm đang bận. Thử lại sau giây lát.", "Recording is busy. Try again shortly.") \
    X(WB_SEND_UNCONFIRMED_HINT, "\n\nChưa xác nhận đã gửi. Hãy xem tin nhắn trợ lý trước khi gửi lại yêu cầu.", "\n\nDelivery is unconfirmed. Check assistant messages before sending the request again.") \
    X(WB_RECOGNITION_INCOMPLETE_HINT, "\n\nNhận dạng chưa xong. Yêu cầu chưa được gửi tới máy tính.", "\n\nRecognition did not finish. The request was not sent to the computer.") \
    X(WB_PREVIOUS_CLOSING, "Kết nối trước đang đóng. Thử lại sau giây lát.", "The previous connection is closing. Try again shortly.") \
    X(WB_CONTEXT_MEMORY, "Không đủ bộ nhớ cho yêu cầu. Thử lại sau giây lát.", "Not enough memory for the request. Try again shortly.") \
    X(WB_TASK_MEMORY, "Không đủ bộ nhớ cho giọng nói hoặc tin nhắn. Thử lại sau giây lát.", "Not enough memory for voice or messages. Try again shortly.") \
    X(WB_HOLD_TO_SPEAK_HINT, "Tiếp tục giữ. Nói sau khi sóng âm hiện ra (ít nhất nửa giây).", "Keep holding. Speak after the listening waveform appears (at least half a second).") \
    X(WB_RECORDING_FINISH_BUSY, "Ghi âm sắp xong. Thử lại sau giây lát.", "Recording is finishing. Try again shortly.") \
    X(WB_MINUTE_SUFFIX, " phút", " min") \
    X(WB_SECOND_SUFFIX, " giây", " sec") \
    X(WB_CONFIRM_PHONE, "Xác nhận liên kết thiết bị trên điện thoại.", "Confirm the device link on your phone.") \
    X(WB_QR_VALIDITY_PREFIX, "Quét bằng điện thoại để liên kết trợ lý\nMã có hiệu lực trong ", "Scan with your phone to link the assistant\nCode valid for ") \
    X(WB_QR_FAILED, "Không tạo được mã. Chạm Làm mới để thử lại.", "Cannot generate code. Tap Refresh to retry.") \
    X(WB_QR_DISPLAY_FAILED, "Không hiển thị được mã", "Cannot display the code") \
    X(WB_RELINK_WAIT, "Đang chuẩn bị liên kết lại. Vui lòng chờ...", "Preparing to relink. Please wait...") \
    X(WB_RECOGNIZING, "Đang nhận dạng...", "Recognizing...") \
    X(WB_SYNCING, "Đang đồng bộ tin nhắn...", "Syncing messages...") \
    X(WB_SENDING, "Đang gửi...", "Sending...") \
    X(WB_PROCESSING, "Trợ lý đang xử lý...", "Assistant is working...") \
    X(WB_NO_MESSAGES, "Không có tin nhắn văn bản để đồng bộ", "No text messages to sync") \
    X(WB_SYNCED, "Đã đồng bộ tin nhắn", "Messages synced") \
    X(WB_LAYOUT_FAILED, "Không dàn được tin nhắn. Thử lại sau giây lát.", "Cannot lay out messages. Try again shortly.") \
    X(WB_RELINK, "Liên kết lại", "Relink") \
    X(WB_RELINK_TITLE, "Liên kết lại tài khoản trợ lý?", "Relink the assistant account?") \
    X(WB_RELINK_WARNING, "Liên kết hiện tại được giữ đến khi tài khoản mới được ủy quyền. Tác vụ đã gửi có thể vẫn chạy.", "The current link is kept until the new account is authorized. Submitted tasks may continue running.") \
    X(WB_RELINK_CANCEL, "Giữ hiện tại", "Keep current") \
    X(WB_RELINK_CONTINUE, "Tiếp tục", "Continue") \
    X(WB_SCAN_LINK, "Quét bằng điện thoại để liên kết thiết bị.", "Scan with your phone to link the device.") \
    X(WB_STOPPED_HINT, "Đã dừng ghi âm hoặc chờ. Tác vụ đã gửi có thể vẫn chạy. Hãy đồng bộ tin nhắn để kiểm tra.", "Recording or waiting stopped. Submitted tasks may continue running. Sync messages to check.") \
    X(WB_STOPPED, "Đã dừng ghi âm hoặc chờ. Bạn có thể tiếp tục trò chuyện.", "Recording or waiting stopped. You can continue chatting.") \
    X(WB_WELCOME_READY, "Sẵn sàng khi bạn cần. Hãy bắt đầu trò chuyện.", "Ready whenever you are. Start a conversation.") \
    X(WB_WELCOME_TASK, "Hôm nay bạn muốn làm gì?", "What would you like to work on today?") \
    X(WB_WELCOME_IDEAS, "Hãy kể cho tôi ý tưởng hoặc kế hoạch của bạn.", "Tell me about your ideas or plans.") \
    X(WB_HISTORY_MEMORY, "Không đủ bộ nhớ cho lịch sử trò chuyện. Thử lại sau giây lát.", "Not enough memory for chat history. Try again shortly.") \
    X(WB_ME_PREFIX, "Tôi: ", "Me: ") \
    X(WB_ASSISTANT_PREFIX, "Trợ lý: ", "Assistant: ") \
    X(AUDIO_SERVICE_START_FAILED, "Dịch vụ âm thanh không khởi động được. Thử lại sau giây lát.", "Audio service failed to start. Try again shortly.") \
    X(WB_STORAGE_TASK_BUSY, "Tác vụ lưu trữ của thiết bị đang bận. Thử lại sau giây lát.", "Device storage task is busy. Try again shortly.") \
    X(WB_STORAGE_BUSY, "Bộ nhớ thiết bị đang bận. Thử lại sau giây lát.", "Device storage is busy. Try again shortly.") \
    X(WB_ENDPOINT_MISSING, "Chưa cấu hình địa chỉ dịch vụ WorkBuddy.", "WorkBuddy service address is not configured.") \
    X(WB_NETWORK_SETTINGS_HINT, "Không có mạng. Hãy kết nối trong Cài đặt trước.", "No network. Connect in Settings first.") \
    X(WB_CONNECTION_CREATE_FAILED, "Không tạo được kết nối mạng. Thử lại sau giây lát.", "Cannot create a network connection. Try again shortly.") \
    X(WB_SERVICE_TIMEOUT, "Kết nối dịch vụ thất bại hoặc hết thời gian chờ. Thử lại sau giây lát.", "Service connection failed or request timed out. Try again shortly.") \
    X(WB_RESPONSE_TIMEOUT, "Dịch vụ phản hồi quá lâu. Thử lại sau giây lát.", "Service response timed out. Try again shortly.") \
    X(WB_RESPONSE_READ_FAILED, "Không đọc được phản hồi dịch vụ. Thử lại sau giây lát.", "Cannot read service response. Try again shortly.") \
    X(WB_RESPONSE_TOO_LONG, "Phản hồi dịch vụ quá dài. Thử lại sau giây lát.", "Service response is too long. Try again shortly.") \
    X(WB_HTTP_ERROR_PREFIX, "Dịch vụ không khả dụng (HTTP ", "Service unavailable (HTTP ") \
    X(WB_RESPONSE_INVALID, "Phản hồi dịch vụ không hợp lệ. Thử lại sau giây lát.", "Invalid service response. Try again shortly.") \
    X(WB_NO_REPLY, "Dịch vụ không trả lời. Thử lại sau giây lát.", "No reply from the service. Try again shortly.") \
    X(WB_VOICE_ENDPOINT_MISSING, "Chưa cấu hình địa chỉ dịch vụ giọng nói trực tiếp.", "Live voice service address is not configured.") \
    X(WB_TOKEN_MISSING_SCAN, "Thiếu thông tin ủy quyền. Hãy quét lại để ủy quyền.", "Authorization credentials are missing. Scan again to authorize.") \
    X(WB_VOICE_ENDPOINT_INVALID, "Địa chỉ dịch vụ giọng nói trực tiếp không hợp lệ", "Invalid live voice service address") \
    X(WB_RECORDING_BUFFER_MEMORY, "Không đủ bộ nhớ đệm ghi âm. Thử lại sau giây lát.", "Not enough recording buffer memory. Try again shortly.") \
    X(WB_TOKEN_NOT_SAVED, "Thiết bị chưa lưu thông tin ủy quyền", "No authorization credentials saved on the device") \
    X(WB_ONLINE_STATUS_INVALID, "Dịch vụ không trả về trạng thái kết nối máy tính hợp lệ.", "Service returned no valid computer connection status.") \
    X(WB_AUTH_STATUS_MISSING, "Dịch vụ không trả về trạng thái ủy quyền thiết bị.", "Service returned no device authorization status.") \
    X(WB_SCAN_CANCELLED, "Đã hủy yêu cầu quét", "Scan request cancelled") \
    X(WB_RELINK_UNSUPPORTED, "Không thể liên kết lại. Hãy nhờ quản trị viên cập nhật dịch vụ.", "Relinking is unavailable. Ask the administrator to update the service.") \
    X(WB_QR_RESPONSE_INVALID, "Dịch vụ trả về thông tin quét không hợp lệ.", "Service returned invalid scan information.") \
    X(WB_AUTH_SAVE_FAILED, "Không lưu được ủy quyền thiết bị. Giải phóng bộ nhớ rồi thử lại.", "Cannot save device authorization. Free storage space and retry.") \
    X(WB_SCAN_SESSION_INVALID, "Phiên quét không hợp lệ. Hãy làm mới mã.", "Invalid scan session. Refresh the code.") \
    X(WB_AUTH_STATUS_INVALID, "Trạng thái ủy quyền không hợp lệ. Hãy làm mới mã.", "Invalid authorization status. Refresh the code.") \
    X(WB_TOKEN_SAVE_FAILED, "Đã ủy quyền nhưng không lưu được thông tin. Mở lại ứng dụng rồi thử lại.", "Authorized, but credentials could not be saved. Reopen the app and retry.") \
    X(WB_ENTER_QUESTION, "Hãy nhập câu hỏi trước.", "Enter a question first.") \
    X(WB_QUESTION_INVALID, "Câu hỏi quá dài hoặc có ký tự không hợp lệ. Hãy rút ngắn rồi thử lại.", "Question is too long or contains invalid characters. Shorten it and retry.") \
    X(WB_AUTHORIZE_FIRST, "Hãy quét để đăng nhập và ủy quyền WorkBuddy trước.", "Scan to sign in and authorize WorkBuddy first.") \
    X(WB_TEXT_MEMORY, "Không đủ bộ nhớ để gửi văn bản.", "Not enough memory to send text.") \
    X(WB_MESSAGE_INVALID, "Định dạng tin nhắn không hợp lệ. Cập nhật dịch vụ rồi thử lại.", "Invalid message format. Update the service and retry.") \
    X(WB_REQUEST_ID_INVALID, "Mã yêu cầu máy tính không hợp lệ. Không thể tự động lấy câu trả lời.", "Invalid computer request ID. Cannot retrieve replies automatically.") \
    X(WB_TOKEN_MISSING, "Thiếu thông tin ủy quyền. Hãy ủy quyền lại.", "Authorization credentials are missing. Authorize again.") \
    X(WB_AUTO_REPLY_UNAVAILABLE, "Không có trả lời tự động. Cập nhật dịch vụ Java hoặc đồng bộ tin nhắn thủ công.", "Automatic replies are unavailable. Update the Java service, or sync messages manually.") \
    X(WB_AUTO_REPLY_MISMATCH, "Câu trả lời không khớp với yêu cầu này. Hãy đồng bộ tin nhắn thủ công sau.", "Reply does not match this request. Sync messages manually later.") \
    X(WB_VOICE_RESPONSE_FORMAT, "Định dạng phản hồi giọng nói trực tiếp không hợp lệ", "Invalid live voice response format") \
    X(WB_VOICE_RESPONSE_INVALID, "Phản hồi giọng nói trực tiếp không hợp lệ", "Invalid live voice response") \
    X(WB_VOICE_SESSION_MISMATCH, "Mã phiên giọng nói trực tiếp không khớp", "Live voice session ID mismatch") \
    X(WB_VOICE_RECOGNITION_FAILED, "Nhận dạng giọng nói thất bại. Hãy nói lại.", "Live voice recognition failed. Speak again.") \
    X(WB_VOICE_TEXT_INVALID, "Văn bản nhận dạng trống, quá dài hoặc sai thứ tự.", "Recognized text is empty, too long or out of order.") \
    X(WB_VOICE_VERSION, "Giao thức giọng nói trực tiếp không tương thích. Cập nhật dịch vụ Java.", "Incompatible live voice protocol. Update the Java service.") \
    X(WB_VOICE_BUFFER_FAILED, "Không cấp được bộ đệm ghi âm trực tiếp", "Cannot allocate live recording buffer") \
    X(WB_VOICE_NETWORK_HINT, "Không có mạng. Kết nối lại trước khi nói.", "No network. Reconnect before speaking.") \
    X(WB_VOICE_CONNECTION_CREATE, "Không tạo được kết nối giọng nói trực tiếp", "Cannot create live voice connection") \
    X(WB_VOICE_TRANSFER_FAILED, "Truyền giọng nói thất bại. Hãy nói lại.", "Live voice transfer failed. Speak again.") \
    X(WB_VOICE_CONNECTION_FAILED, "Kết nối giọng nói thất bại. Kiểm tra phiên bản dịch vụ Java, proxy WebSocket và ủy quyền thiết bị.", "Live voice connection failed. Check the Java service version, WebSocket proxy and device authorization.") \
    X(WB_VOICE_EARLY_END, "Nhận dạng kết thúc sớm. Chưa gửi yêu cầu.", "Recognition ended early. Request not sent.") \
    X(WB_VOICE_DISCONNECTED, "Mất kết nối giọng nói. Chưa gửi yêu cầu. Hãy nói lại.", "Voice disconnected. Request not sent. Speak again.") \
    X(WB_VOICE_READY_TIMEOUT, "Dịch vụ giọng nói chưa sẵn sàng kịp. Thử lại sau giây lát.", "Voice service did not become ready in time. Try again shortly.") \
    X(WB_VOICE_FINAL_TIMEOUT, "Hết thời gian chờ kết quả nhận dạng. Hãy nói lại.", "Final recognition result timed out. Speak again.") \
    X(WB_VOICE_OVERFLOW, "Bộ đệm âm thanh chờ gửi đã đầy. Kiểm tra mạng rồi thử lại.", "Pending audio buffer is full. Check the network and retry.") \
    X(WB_VOICE_BUFFER_UNAVAILABLE, "Bộ đệm ghi âm trực tiếp không dùng được. Hãy mở lại ứng dụng.", "Live recording buffer is unavailable. Reopen the app.") \
    X(WB_VOICE_PACKET_INVALID, "Dữ liệu ghi âm không hợp lệ. Hãy mở lại ứng dụng.", "Recording data is invalid. Reopen the app.") \
    X(WB_VOICE_SEND_FAILED, "Gửi âm thanh thất bại. Hãy nói lại.", "Live audio send failed. Speak again.") \
    X(WB_AUDIO_CAPTURE_FAILED, "Thu hoặc mã hóa âm thanh thất bại. Hãy nói lại.", "Audio capture or encoding failed. Speak again.") \
    X(WB_VOICE_TOO_SHORT, "Bản ghi ngắn hơn nửa giây. Hãy giữ và nói lâu hơn.", "Recording is shorter than half a second. Hold and speak longer.") \
    X(WB_VOICE_INCOMPLETE, "Số byte âm thanh đã gửi không khớp bản ghi. Hãy nói lại.", "Sent audio byte count does not match the recording. Speak again.") \
    X(WB_VOICE_FINISH_FAILED, "Không gửi được xác nhận kết thúc giọng nói", "Cannot send end-of-voice confirmation") \
    X(WB_VOICE_CANCELLED, "Đã hủy giọng nói. Chưa gửi yêu cầu.", "Voice cancelled. Request not sent.") \
    X(BOOK_BOOKMARK_LAYOUT_BUSY, "Đang dàn trang. Hãy thêm dấu trang sau giây lát.", "Laying out text. Add the bookmark shortly.") \
    X(BOOK_BOOKMARK_ADD_MEMORY, "Không đủ bộ nhớ để thêm dấu trang", "Not enough memory to add a bookmark") \
    X(BOOK_BOOKMARK_LOCATE_FAILED, "Không tìm được dấu trang. Kiểm tra xem sách có thay đổi không.", "Cannot locate bookmark. Check whether the book has changed.") \
    X(ASSISTANT_AI_PREFIX, "AI: ", "AI: ") \
    X(BOOK_BOOKMARK_IMAGE_SUFFIX, " (Trang ảnh hoặc trang chương)", " (Image or chapter page)") \
    X(BOOK_BOOKMARK_IMAGE, "Trang ảnh hoặc trang chương", "Image or chapter page") \
    X(WIFI_DISABLED_RETRY, "Wi-Fi đang tắt. Hãy bật rồi thử lại.", "Wi-Fi is off. Turn it on and try again.") \
    X(AUDIO_INIT_FAILED, "Khởi tạo âm thanh thất bại", "Audio initialization failed") \
    X(SETTINGS_TEST_CAMERA_DETECTED, "Đã phát hiện", "Detected") \
    X(SETTINGS_TEST_CAMERA_NOT_DETECTED, "Không phát hiện", "Not detected") \
    X(SETTINGS_TEST_CAMERA_SHOT, "Chụp", "Capture") \
    X(SETTINGS_TEST_CAMERA_RESULT, "Kết quả camera", "Camera result") \
    X(SETTINGS_TEST_CAPTURING, "Đang chụp…", "Capturing…") \
    X(SETTINGS_TEST_DETECTING, "Đang dò…", "Detecting…") \
    X(SETTINGS_TEST_TASK_CREATE_FAIL, "Tạo tác vụ thất bại", "Task creation failed") \
    X(SETTINGS_TEST_CELL_EXT, "4G·Ngoài", "4G·External") \
    X(SETTINGS_TEST_CELL_INT, "4G·Trong", "4G·Internal") \
    X(SETTINGS_TEST_SCANNING, "Đang quét…", "Scanning…") \
    X(SETTINGS_TEST_WIFI_CLOSE, "Đóng", "Close") \
    X(SETTINGS_TEST_WIFI_COUNT_FMT, "%d mạng", "%d network(s)") \
    X(SETTINGS_TEST_WIFI_FOUND_FMT, "Tìm thấy %d", "Found %d") \
    X(SETTINGS_TEST_WIFI_NEARBY, "Wi-Fi gần đây", "Nearby Wi-Fi") \
    X(SETTINGS_TEST_WIFI_NEXT, "Sau", "Next") \
    X(SETTINGS_TEST_WIFI_NONE, "Không có mạng", "No networks") \
    X(SETTINGS_TEST_WIFI_PREV, "Trước", "Prev") \
    X(SETTINGS_TEST_WIFI_RESCAN, "Quét lại", "Rescan") \
    X(SETTINGS_TEST_WIFI_SCANNING, "Đang quét…", "Scanning…") \
    X(SETTINGS_TEST_WIFI_SCAN_FAIL, "Quét thất bại", "Scan failed") \
    X(REGISTERING_NETWORK, "Đang chờ mạng...", "Waiting for network...") \
    X(SETTINGS_NET_BOARD_NO_AP, "Bo mạch này không có chế độ cấu hình Wi-Fi", "This board has no Wi-Fi config mode") \
    X(SETTINGS_NET_CFUN0, "Đang tắt sóng…\nAT+CFUN=0", "RF off…\nAT+CFUN=0") \
    X(SETTINGS_NET_CFUN0_FAIL, "AT+CFUN=0 thất bại", "AT+CFUN=0 failed") \
    X(SETTINGS_NET_CFUN1, "Đang tìm mạng…\nAT+CFUN=1", "Searching network…\nAT+CFUN=1") \
    X(SETTINGS_NET_CFUN1_FAIL, "Không khôi phục được 4G. Thử lại hoặc khởi động lại thiết bị.", "Could not restore 4G. Retry or restart the device.") \
    X(SETTINGS_NET_CURRENT_FMT, "Hiện tại: %s", "Current: %s") \
    X(SETTINGS_NET_CUR_DASH, "Hiện tại: --", "Current: --") \
    X(SETTINGS_NET_ECSIMCFG_FAIL, "AT+ECSIMCFG thất bại", "AT+ECSIMCFG failed") \
    X(SETTINGS_NET_MODE_4G, "Di động", "Cellular") \
    X(SETTINGS_NET_MODE_FMT, "Chế độ mạng: %s", "Network mode: %s") \
    X(SETTINGS_NET_WIFI_STATUS_TITLE, "Trạng thái Wi-Fi:", "Wi-Fi status:") \
    X(SETTINGS_NET_WIFI_ENABLED, "Đã bật", "Enabled") \
    X(SETTINGS_NET_MODE_WIFI, "Wi-Fi", "Wi-Fi") \
    X(SETTINGS_NET_NOT_4G, "Không ở chế độ 4G, không thể chuyển", "Not in 4G mode, cannot switch") \
    X(SETTINGS_NET_NO_4G, "Không có mô-đun 4G", "No 4G module") \
    X(SETTINGS_NET_REBOOT_HINT, "Thiết bị khởi động lại sau khi chuyển", "Device reboots after switching") \
    X(SETTINGS_NET_SIM_EXT, "SIM ngoài", "External SIM") \
    X(SETTINGS_NET_SIM_INT, "SIM trong", "Internal SIM") \
    X(SETTINGS_NET_SIM_TASK_FAIL, "Không bắt đầu được tác vụ chuyển SIM", "Cannot start SIM switch task") \
    X(SETTINGS_NET_SIM_TITLE, "SIM", "SIM") \
    X(SETTINGS_NET_SWITCHED_FMT, "Đã chuyển sang %s", "Switched to %s") \
    X(SETTINGS_NET_SWITCH_4G, "Đang chuyển sang 4G…", "Switching to 4G…") \
    X(SETTINGS_NET_SWITCH_SIM_CFUN0_FMT, "Đang chuyển sang %s…\nAT+CFUN=0", "Switching to %s…\nAT+CFUN=0") \
    X(SETTINGS_NET_SWITCH_SIM_FMT, "Đang chuyển sang %s…\n%s", "Switching to %s…\n%s") \
    X(SETTINGS_NET_SWITCH_WIFI, "Đang chuyển sang Wi-Fi…", "Switching to Wi-Fi…") \
    X(SETTINGS_TEST_CELL_CANCELLED, "Đã hủy kiểm tra", "Test cancelled") \
    X(SETTINGS_TEST_CELL_CFUN0_FAIL, "CFUN=0 thất bại", "CFUN=0 failed") \
    X(SETTINGS_TEST_CELL_CFUN1_FAIL, "CFUN=1 thất bại", "CFUN=1 failed") \
    X(SETTINGS_TEST_CELL_LOSS_FMT, "Mất %d/%d", "Loss %d/%d") \
    X(SETTINGS_TEST_CELL_MODEM_NOT_READY, "Modem chưa sẵn sàng", "Modem not ready") \
    X(SETTINGS_TEST_CELL_MODEM_NO_RESP, "Modem không phản hồi", "Modem no response") \
    X(SETTINGS_TEST_CELL_NEED_4G, "Hãy chuyển sang chế độ 4G", "Switch to 4G mode") \
    X(SETTINGS_TEST_CELL_NO_RESOURCE, "Hết tài nguyên", "Out of resources") \
    X(SETTINGS_TEST_CELL_NO_SIM, "Không phát hiện SIM", "No SIM detected") \
    X(SETTINGS_TEST_CELL_PING_CMD_FAIL, "Lệnh PING thất bại", "PING command failed") \
    X(SETTINGS_TEST_CELL_PING_FAIL, "PING thất bại", "PING failed") \
    X(SETTINGS_TEST_CELL_PING_OK, "PING OK", "PING OK") \
    X(SETTINGS_TEST_CELL_PING_WAIT_TIMEOUT, "Hết thời gian chờ PING", "PING wait timeout") \
    X(SETTINGS_TEST_CELL_REG_REJECT, "Đăng ký mạng bị từ chối", "Registration rejected") \
    X(SETTINGS_TEST_CELL_SEARCH_TIMEOUT, "Hết thời gian tìm mạng", "Network search timeout") \
    X(SETTINGS_TEST_CELL_SIM_SWITCH_FAIL, "Chuyển SIM thất bại", "SIM switch failed") \
    X(SETTINGS_TEST_CELL_SLOT_FAIL, "Chưa áp dụng khe SIM", "SIM slot not applied") \
    X(SETTINGS_TEST_CELL_TESTING, "Đang kiểm tra…", "Testing…") \
    X(SETTINGS_TEST_CELL_TEST_FAIL, "Kiểm tra thất bại", "Test failed") \
    X(SETTINGS_TEST_CELL_WAIT_RESOURCE, "Đang chờ tài nguyên…", "Waiting for resources…") \
    X(SETTINGS_TEST_SIGNAL_BOARD_UNSUP, "Bo mạch không hỗ trợ", "Board unsupported") \
    X(SETTINGS_TEST_SIGNAL_CELL_CSQ_FMT, "4G CSQ %2d", "4G CSQ %2d") \
    X(SETTINGS_TEST_SIGNAL_CELL_CSQ_UNKNOWN, "4G CSQ không rõ", "4G CSQ unknown") \
    X(SETTINGS_TEST_SIGNAL_CELL_UNAVAIL, "4G không khả dụng", "4G unavailable") \
    X(SETTINGS_TEST_SIGNAL_ICCID_TITLE, "ICCID:", "ICCID:") \
    X(SETTINGS_TEST_SIGNAL_ICCID_UNKNOWN, "--", "--") \
    X(SETTINGS_TEST_SIGNAL_READING, "Đang đọc…", "Reading…") \
    X(SETTINGS_TEST_SIGNAL_TITLE, "Tín hiệu", "Signal") \
    X(SETTINGS_TEST_SIGNAL_WIFI_DISCONNECTED, "Wi-Fi mất kết nối", "Wi-Fi disconnected") \
    X(SETTINGS_TEST_SIGNAL_WIFI_READ_FAIL, "Đọc Wi-Fi thất bại", "Wi-Fi read failed") \
    X(SETTINGS_TEST_SIGNAL_WIFI_RSSI_FMT, "Wi-Fi %d dBm", "Wi-Fi %d dBm") \
    X(SWITCH_TO_4G_NETWORK, "Đang chuyển sang 4G...", "Switching to 4G...") \
    X(SWITCH_TO_WIFI_NETWORK, "Đang chuyển sang Wi-Fi...", "Switching to Wi-Fi...") \
    X(SETTINGS_TAB_LOG, "Nhật ký", "Logs") \
    X(SETTINGS_LOG_TITLE, "Nhật ký thiết bị", "Device logs") \
    X(SETTINGS_LOG_ON, "Bật", "On") \
    X(SETTINGS_LOG_OFF, "Tắt", "Off") \
    X(SETTINGS_LOG_CURRENT_ON, "Hiện tại: Bật", "Current: On") \
    X(SETTINGS_LOG_CURRENT_OFF, "Hiện tại: Tắt", "Current: Off") \
    X(SETTINGS_LOG_HINT, "Khi bật, nhật ký được lưu vào SD: metalio/e-ink/logs/metalio_logs.txt", "When on, runtime logs are saved to SD: metalio/e-ink/logs/metalio_logs.txt") \
    X(SETTINGS_LOG_HINT_OFF, "Tắt sẽ dừng ghi. Ghi SD tạm dừng khi bật ổ đĩa USB", "Off stops capture. SD writing pauses while USB disk is enabled") \
    X(SETTINGS_LOG_CLEAR, "Xóa nhật ký", "Clear logs") \
    X(SETTINGS_LOG_CLEARING, "Đang xóa…", "Clearing…") \
    X(SETTINGS_LOG_CLEAR_OK, "Đã xóa nhật ký", "Logs cleared") \
    X(SETTINGS_LOG_CLEAR_FAIL, "Xóa thất bại. Kiểm tra thẻ SD hoặc thử lại.", "Clear failed. Check SD card or try again.") \
    X(SETTINGS_LOG_UPLOAD, "Tải nhật ký lên", "Upload logs") \
    X(SETTINGS_LOG_CONNECTING_NET, "Đang kết nối mạng", "Connecting to network") \
    X(SETTINGS_LOG_UPLOAD_COUNT_FMT, "Xong %u, còn %u", "Completed %u, remaining %u") \
    X(SETTINGS_LOG_UPLOADING, "Đang tải nhật ký lên", "Uploading logs") \
    X(SETTINGS_LOG_UPLOAD_OK, "Đã tải nhật ký lên", "Logs uploaded") \
    X(SETTINGS_LOG_UPLOAD_FAIL, "Tải lên thất bại. Vui lòng thử lại", "Upload failed. Please retry") \
    X(SETTINGS_LOG_UPLOAD_NO_FILE, "Không có nhật ký để tải lên", "No logs to upload") \
    X(SETTINGS_LOG_CANCELLING, "Đang hủy, vui lòng chờ", "Cancelling, please wait") \
    X(SETTINGS_LOG_PREPARING, "Đang chuẩn bị nhật ký", "Preparing logs") \
    X(SETTINGS_LOG_SD_UNAVAILABLE, "Không dùng được SD. Tắt ổ đĩa USB rồi thử lại", "SD unavailable. Disable USB disk and retry") \
    X(SETTINGS_LOG_NO_MEMORY, "Không đủ bộ nhớ. Vui lòng thử lại sau", "Not enough memory. Please retry later") \
    X(HOME_APP_TASK, "Công việc", "Tasks")                                                                      \
    X(HOME_APP_ASSISTANT, "Hỏi AI", "Ask AI")                                                                  \
    X(HOME_APP_BOOK, "Sách", "Books")                                                                          \
    X(HOME_APP_WALLPAPER, "Hình nền", "Walls")                                                                     \
    X(HOME_APP_CLOUD, "Truyền tệp", "Transfer")                                                                      \
    X(HOME_APP_SETTINGS, "Cài đặt", "Settings")                                                                   \
    X(HOME_DATE_PLACEHOLDER, "--/--", "--/--")                                                              \
    X(HOME_DATE_FMT, "%02d/%02d", "%02d/%02d")                                                              \
    X(HOME_DATE_SLASH_PLACEHOLDER, "----.-.--", "----.-.--")                                                   \
    X(HOME_DATE_SLASH_FMT, "%d.%d.%d", "%d.%d.%d")                                                             \
    X(HOME_WDAY_SUN, "Chủ nhật", "Sun")                                                                            \
    X(HOME_WDAY_MON, "Thứ hai", "Mon")                                                                            \
    X(HOME_WDAY_TUE, "Thứ ba", "Tue")                                                                            \
    X(HOME_WDAY_WED, "Thứ tư", "Wed")                                                                            \
    X(HOME_WDAY_THU, "Thứ năm", "Thu")                                                                            \
    X(HOME_WDAY_FRI, "Thứ sáu", "Fri")                                                                            \
    X(HOME_WDAY_SAT, "Thứ bảy", "Sat")                                                                            \
    X(BATTERY_NEED_CHARGE, "Pin yếu, vui lòng sạc", "Low battery, please charge")                                   \
    X(BATTERY_SHUTTING_DOWN, "Pin quá yếu. Đang tắt máy.", "Battery critically low. Shutting down.")              \
    X(BATTERY_LOW_VOLTAGE_SHUTTING_DOWN, "Điện áp pin quá thấp. Đang tắt máy.", "Battery voltage critically low. Shutting down.") \
    X(COMMON_UNKNOWN, "Không rõ", "Unknown")                                                                       \
    X(POWERED_OFF, "Đã tắt máy", "Powered off")                                                                    \
    X(SETTINGS_TAB_NETWORK, "Mạng", "Network")                                                                 \
    X(SETTINGS_TAB_THEME, "Giao diện", "Theme")                                                                     \
    X(SETTINGS_TAB_LANGUAGE, "Ngôn ngữ", "Language")                                                               \
    X(SETTINGS_TAB_HAPTIC, "Rung", "Haptic")                                                                   \
    X(SETTINGS_TAB_CHARGING, "Sạc", "Charging") \
    X(SETTINGS_CHARGING_CURRENT, "Dòng sạc", "Charge current") \
    X(SETTINGS_CHARGING_HINT, "Mặc định: 1A. Lựa chọn được lưu tự động.", "Default: 1A. Selection is saved automatically.") \
    X(SETTINGS_CHARGING_FAILED, "Không áp dụng được cài đặt. Hãy thử lại.", "Could not apply setting. Try again.") \
    X(SETTINGS_TAB_POWER, "Nguồn", "Power")                                                                     \
    X(SETTINGS_SCREEN_BLACK_DEPTH_TITLE, "Độ đậm màu đen", "Black depth") \
    X(SETTINGS_TEST_VCOM_QUERY, "Truy vấn", "Query") \
    X(SETTINGS_TEST_BT_VERSION, "Phiên bản BT", "BT version") \
    X(SETTINGS_TEST_SD_CAP_FMT, "Trống %.1fGB / Tổng %.1fGB", "Free %.1fGB / Total %.1fGB") \
    X(SETTINGS_TEST_POWER_OFF, "Tắt máy", "Power off") \
    X(SETTINGS_SCREEN_BLACK_DEPTH_DEFAULT, "Mặc định", "Default") \
    X(SETTINGS_SCREEN_BLACK_DEPTH_MEDIUM, "Vừa", "Medium") \
    X(SETTINGS_SCREEN_BLACK_DEPTH_STRONG, "Đậm", "Strong") \
    X(SETTINGS_SCREEN_BLACK_DEPTH_HINT, "Tăng độ đậm màu đen có thể ảnh hưởng tuổi thọ màn hình và làm nền đổi màu. Nên dùng Mặc định.", "Increasing black depth may affect display lifespan and cause background discoloration. Default is recommended.") \
    X(SETTINGS_VCOM_UNKNOWN, "Không rõ", "Unknown") \
    X(SETTINGS_VCOM_WRITE_OK, "Đã lưu VCOM", "VCOM saved") \
    X(SETTINGS_VCOM_READ_OK, "Đọc VCOM thành công", "VCOM read successfully") \
    X(SETTINGS_VCOM_WRITE_FAILED, "Ghi VCOM thất bại; kiểm tra chip Bluetooth", "VCOM write failed; check the Bluetooth chip") \
    X(SETTINGS_VCOM_READ_FAILED, "Đọc VCOM thất bại; kiểm tra chip Bluetooth", "VCOM read failed; check the Bluetooth chip") \
    X(SETTINGS_VCOM_INVALID, "Giá trị bất thường: độ lớn VCOM phải từ 1.0–3.0V, bước 10mv", "Abnormal value: VCOM magnitude must be 1.0–3.0V, in 10mv steps") \
    X(SETTINGS_TEST_MARGIN, "Căn chỉnh giao diện", "UI adjustment") \
    X(SETTINGS_SCREEN_MARGIN_TITLE, "Dịch giao diện (-20~20px): Áp dụng và lưu ngay.", "UI offset (-20~20px): Applied and saved immediately.") \
    X(SETTINGS_SCREEN_MARGIN_SET_OK, "Đã lưu cài đặt", "Settings saved") \
    X(SETTINGS_SCREEN_MARGIN_RESET_OK, "Đã khôi phục mặc định", "Defaults restored") \
    X(SETTINGS_SCREEN_MARGIN_SAVE_FAILED, "Không lưu được. Hãy thử lại.", "Could not save. Try again.") \
    X(SETTINGS_SCREEN_MARGIN_SET_DEFAULT, "Đặt làm mặc định", "Set as default") \
    X(SETTINGS_SCREEN_MARGIN_RESET, "Khôi phục mặc định", "Restore defaults") \
    X(SETTINGS_SCREEN_MARGIN_VALUE_FMT, "Giá trị hiện tại: %d", "Current value: %d") \
    X(SETTINGS_SCREEN_MARGIN_TOP, "Lên", "Up") \
    X(SETTINGS_SCREEN_MARGIN_BOTTOM, "Xuống", "Down") \
    X(SETTINGS_SCREEN_MARGIN_LEFT, "Trái", "Left") \
    X(SETTINGS_SCREEN_MARGIN_RIGHT, "Phải", "Right") \
    X(SETTINGS_SCREEN_REFRESH_TITLE, "Chế độ làm mới", "Refresh mode") \
    X(SETTINGS_SCREEN_REFRESH_CLEAR, "Rõ nét", "Clear") \
    X(SETTINGS_SCREEN_REFRESH_CLEAR_HINT, "Chi tiết sắc nét hơn, ít bóng mờ hơn.", "Sharper detail, less ghosting.") \
    X(SETTINGS_SCREEN_REFRESH_BALANCED_HINT, "Cân bằng chất lượng và tốc độ; có thể tăng bóng mờ.", "Balanced quality and speed; may increase ghosting.") \
    X(SETTINGS_SCREEN_REFRESH_BALANCED, "Cân bằng", "Balanced") \
    X(SETTINGS_SCREEN_REFRESH_FAST, "Nhanh", "Fast") \
    X(SETTINGS_SCREEN_REFRESH_FAST_HINT, "Phản hồi nhanh hơn, giữ mức xám; có thể tăng bóng mờ.", "Faster response with grayscale retained; may increase ghosting.") \
    X(SETTINGS_TAB_FRONTLIGHT, "Màn hình", "Screen")                                                                \
    X(SETTINGS_TAB_CONVERSATION, "Trò chuyện", "Talk")                                                               \
    X(SETTINGS_TAB_STORAGE, "Lưu trữ", "Storage")                                                                 \
    X(SETTINGS_TAB_BLUETOOTH, "Bluetooth", "Bluetooth")                                                             \
    X(SETTINGS_PAGER, "Bút lật trang", "Page turner") \
    X(PAGER_SCANNING_SHORT, "Đang quét thiết bị gần đây…", "Scanning nearby devices…") \
    X(PAGER_SAVING, "Đang lưu…", "Saving…") \
    X(PAGER_AUTO, "Tự kết nối bút lật trang", "Auto-connect page turner") \
    X(PAGER_ENABLED, "Đã bật", "Enabled") \
    X(PAGER_AUTO_HINT, "Tìm bút lật trang gần đây khi khởi động và sau chế độ chờ. Sẽ tắt nếu không có kết nối.", "Find nearby page turners at startup and after standby. Turns off if no connection is available.") \
    X(PAGER_AUTO_TIMEOUT, "Không có kết nối. Đã tắt tự kết nối.", "No available connection. Auto-connect is off.") \
    X(PAGER_CURRENT, "Kết nối hiện tại", "Current connection") \
    X(PAGER_NO_CURRENT, "Chưa kết nối bút lật trang", "No page turner connected") \
    X(PAGER_NEARBY, "Gần đây", "Nearby") \
    X(PAGER_AWAY, "Không ở gần", "Not nearby") \
    X(PAGER_CONNECTED_TAG, "Đã kết nối", "Connected") \
    X(PAGER_CONNECTING_TAG, "Đang kết nối", "Connecting") \
    X(PAGER_FAILED_TAG, "Thất bại", "Failed") \
    X(PAGER_NOT_NEARBY, "Thiết bị không ở gần. Làm mới rồi thử lại.", "Device not nearby. Refresh and try again.") \
    X(PAGER_SAVED_FMT, "Thiết bị đã lưu (%d)", "Saved devices (%d)") \
    X(PAGER_UNSAVED_FMT, "Gần đây, chưa lưu (%d)", "Nearby, not saved (%d)") \
    X(PAGER_NO_UNSAVED, "Không có bút lật trang mới ở gần", "No new page turners nearby") \
    X(PAGER_RESCAN, "Làm mới", "Refresh") \
    X(PAGER_LEARNING_FMT, "Đang thiết lập: %s", "Setting up: %s") \
    X(SETTINGS_PAGER_MANAGE, "Thiết bị", "Devices") \
    X(SETTINGS_PAGER_CUSTOMIZE, "Gán nút", "Button mapping") \
    X(SETTINGS_PAGER_NO_SAVED, "Chưa có thiết bị đã lưu. Kết nối một bút lật trang gần đây để lưu.", "No saved devices. Connect a nearby page turner to save it.") \
    X(SETTINGS_PAGER_CONNECT, "Kết nối", "Connect") \
    X(SETTINGS_PAGER_FORGET, "Quên thiết bị", "Forget device") \
    X(SETTINGS_PAGER_FORGET_HINT, "Quên thiết bị này sẽ xóa thông tin ghép đôi và cách gán nút.", "Forgetting this device removes its pairing information and button mappings.") \
    X(SETTINGS_PAGER_PREVIOUS, "Trang trước", "Previous page") \
    X(SETTINGS_PAGER_NEXT, "Trang sau", "Next page") \
    X(SETTINGS_PAGER_NEED_CONNECTION, "Hãy kết nối bút lật trang trong mục Thiết bị trước.", "Connect a page turner in Devices first.") \
    X(SETTINGS_PAGER_BINDING, "%s%sNút %c", "%s%sButton %c") \
    X(SETTINGS_PAGER_UNSET, "Chưa đặt", "Not set") \
    X(SETTINGS_PAGER_PAGE_HINT, "Mặc định: A trang trước, B trang sau.\nKhông bắt buộc phải học nút.", "Defaults: A previous, B next.\nLearning is optional.") \
    X(SETTINGS_PAGER_PRESS, "Bấm nút bạn muốn dùng.", "Tap the button you want to use.") \
    X(SETTINGS_PAGER_RELEARN, "Học lại", "Learn again") \
    X(SETTINGS_PAGER_CLEAR, "Xóa gán nút", "Clear mapping") \
    X(SETTINGS_PAGER_SAVE, "Lưu", "Save") \
    X(SETTINGS_PAGER_SAVE_ERROR, "Thao tác thất bại. Hãy thử lại.", "Operation failed. Try again.") \
    X(SETTINGS_PAGER_LEARNED, "Đã nhận: nút %c\nThao tác: %s%s", "Detected: button %c\nAction: %s%s") \
    X(SETTINGS_PAGER_REPLACE_HINT, "\nThao tác này thay cách gán lật trang khác của nút này.", "\nThis replaces the other page-turn mapping for this button.") \
    X(SETTINGS_PAGER_TIMEOUT, "Không nhận được nút nào. Hãy thử học lại.", "No button detected. Try learning again.") \
    X(SETTINGS_PAGER_DISCONNECT, "Ngắt kết nối", "Disconnect") \
    X(SETTINGS_PAGER_STARTING, "Đang bật Bluetooth…", "Starting Bluetooth…") \
    X(SETTINGS_PAGER_EMPTY, "Không tìm thấy bút lật trang. Bật nó lên rồi quét lại.", "No page turner found. Turn it on and scan again.") \
    X(SETTINGS_TAB_TEST, "Kiểm tra", "Test")                                                                       \
    X(SETTINGS_TAB_ABOUT, "Giới thiệu", "About")                                                                     \
    X(SETTINGS_THEME_TITLE, "Giao diện màn hình chính", "Home theme")                                                          \
    X(SETTINGS_THEME_HINT, "Áp dụng khi bạn về màn hình chính", "Applies when you return home")                               \
    X(SETTINGS_SYSTEM_FONT_TITLE, "Phông hệ thống", "System font") \
    X(SETTINGS_SYSTEM_FONT_BUILTIN, "Có sẵn (Literata)", "Built-in (Literata)") \
    X(SETTINGS_SYSTEM_FONT_LOADING, "Đang tải phông…", "Loading font…") \
    X(SETTINGS_SYSTEM_FONT_MISSING, "Không tìm thấy phông, đang dùng Literata", "Font not found, using Literata") \
    X(SYSTEM_FONT_LOAD_FAILED, "Không tải được phông", "Could not load the font") \
    X(SETTINGS_THEME_PIXEL, "Pixel Vision", "Pixel Vision") \
    X(SETTINGS_THEME_CURRENT_PIXEL, "Hiện tại: Pixel Vision", "Current: Pixel Vision") \
    X(HOME_PIXEL_TODAY, "Hôm nay", "Today") \
    X(HOME_PIXEL_PENDING, "Đang chờ", "Pending") \
    X(HOME_PIXEL_DAY_NOTE, "Một ngày tốt lành", "A good day") \
    X(HOME_PIXEL_READ, "Đọc", "Read") \
    X(HOME_PIXEL_NOTES, "Ghi chú", "Notes") \
    X(SETTINGS_THEME_WHITE, "Trắng", "White")                                                                   \
    X(SETTINGS_THEME_GRAY, "Xám chấm", "Dotted gray")                                                          \
    X(SETTINGS_THEME_BORDER, "Viền đen", "Black border")                                                       \
    X(SETTINGS_THEME_SLASH, "Lưới chéo", "Slash grid")                                                          \
    X(SETTINGS_THEME_CURRENT_WHITE, "Hiện tại: Trắng", "Current: White")                                            \
    X(SETTINGS_THEME_CURRENT_GRAY, "Hiện tại: Xám chấm", "Current: Dotted gray")                                   \
    X(SETTINGS_THEME_CURRENT_BORDER, "Hiện tại: Viền đen", "Current: Black border")                                \
    X(SETTINGS_THEME_CURRENT_SLASH, "Hiện tại: Lưới chéo", "Current: Slash grid")                                   \
    X(SETTINGS_LANG_TITLE, "Ngôn ngữ", "Language")                                                             \
    X(SETTINGS_LANG_HINT, "Có hiệu lực ngay", "Takes effect immediately")                                        \
    X(SETTINGS_LANG_VI_VN, "Tiếng Việt", "Tiếng Việt")                                                         \
    X(SETTINGS_LANG_EN_US, "English", "English")                                                               \
    X(SETTINGS_LANG_CURRENT_VI, "Hiện tại: Tiếng Việt", "Current: Tiếng Việt")                                         \
    X(SETTINGS_LANG_CURRENT_EN, "Hiện tại: English", "Current: English")                                           \
    X(SETTINGS_HAPTIC_TITLE, "Rung khi bấm nút", "Button haptic")                                                      \
    X(SETTINGS_HAPTIC_HINT, "Nút trên giao diện và phím trên vỏ", "UI buttons and cover keys")                                   \
    X(SETTINGS_HAPTIC_ON, "Bật", "On")                                                                        \
    X(SETTINGS_HAPTIC_OFF, "Tắt", "Off")                                                                      \
    X(SETTINGS_HAPTIC_CURRENT_ON, "Hiện tại: Bật", "Current: On")                                                 \
    X(SETTINGS_HAPTIC_CURRENT_OFF, "Hiện tại: Tắt", "Current: Off")                                               \
    X(SETTINGS_HAPTIC_INTENSITY_TITLE, "Độ mạnh rung", "Haptic strength") \
    X(SETTINGS_HAPTIC_WEAK, "Nhẹ", "Weak") \
    X(SETTINGS_HAPTIC_MEDIUM, "Vừa", "Medium") \
    X(SETTINGS_HAPTIC_STRONG, "Mạnh", "Strong") \
    X(SETTINGS_HAPTIC_CURRENT_WEAK, "Hiện tại: Nhẹ", "Current: Weak") \
    X(SETTINGS_HAPTIC_CURRENT_MEDIUM, "Hiện tại: Vừa", "Current: Medium") \
    X(SETTINGS_HAPTIC_CURRENT_STRONG, "Hiện tại: Mạnh", "Current: Strong") \
    X(SETTINGS_POWER_30_SEC, "30 giây", "30 s")                                                                  \
    X(SETTINGS_POWER_60_SEC, "60 giây", "60 s")                                                                  \
    X(SETTINGS_POWER_120_SEC, "120 giây", "120 s")                                                               \
    X(SETTINGS_POWER_OFF_NEVER, "Không bao giờ", "Never")                                                               \
    X(SETTINGS_POWER_IDLE_MHZ_TITLE, "CPU khi rảnh (mặc định 320 MHz)", "Idle CPU (def 320 MHz)")                       \
    X(SETTINGS_POWER_IDLE_MHZ_HINT, "Xung nhịp CPU khi rảnh", "CPU clock when idle")                            \
    X(SETTINGS_POWER_NET_GRACE_TITLE, "Giữ kết nối (mặc định 30 giây)", "Keep-alive (def 30 s)")                        \
    X(SETTINGS_POWER_NET_GRACE_HINT, "Thời gian giữ mạng sau lần dùng cuối; chế độ chờ sẽ ngắt kết nối", "Time online after last need; standby disconnects") \
    X(SETTINGS_POWER_STANDBY_TITLE, "Ngủ nhẹ (mặc định 5 phút)", "Light sleep (def 5 min)")                       \
    X(SETTINGS_POWER_STANDBY_HINT, "Vào ngủ nhẹ sau 5 phút không thao tác", "Enters light sleep after 5 minutes of inactivity")                     \
    X(SETTINGS_POWER_OFF_TITLE, "Tự tắt máy (mặc định 30 phút)", "Auto power-off (def 30 min)")                      \
    X(SETTINGS_POWER_OFF_HINT, "Tắt máy sau 30 phút ở chế độ chờ", "Powers off after 30 minutes in standby")                      \
    X(SETTINGS_POWER_SCREEN_WALLPAPER, "Hình nền", "Wallpaper") \
    X(SETTINGS_POWER_SCREEN_COVER, "Bìa sách", "Book cover") \
    X(SETTINGS_CONV_TITLE, "Phát giọng nói", "Voice playback")                                                       \
    X(SETTINGS_CONV_HINT, "Hỏi AI có đọc to câu trả lời không\nTắt = chỉ văn bản", "Whether Ask AI speaks aloud\nOff = text only") \
    X(SETTINGS_CONV_TTS_ON, "Bật đọc", "TTS on")                                                              \
    X(SETTINGS_CONV_TTS_OFF, "Tắt đọc", "TTS off")                                                            \
    X(SETTINGS_CONV_CUR_ON, "Hiện tại: Bật đọc", "Current: TTS on")                                               \
    X(SETTINGS_CONV_CUR_OFF, "Hiện tại: Tắt đọc", "Current: TTS off")                                             \
    X(SETTINGS_CONV_CUR_DASH, "Hiện tại: —", "Current: —")                                                         \
    X(SETTINGS_CONV_PARSE_FAIL, "Đọc phản hồi thất bại", "Response parse failed")                                       \
    X(SETTINGS_CONV_REQUEST_FAIL, "Yêu cầu thất bại", "Request failed")                                                \
    X(SETTINGS_CONV_MISSING_ENABLED, "Thiếu trường enabled", "Missing enabled")                                        \
    X(SETTINGS_CONV_NO_NETWORK, "Không có mạng", "No network")                                                         \
    X(SETTINGS_CONV_CONN_FAIL, "Kết nối thất bại", "Connection failed")                                           \
    X(SETTINGS_CONV_NET_NOT_READY, "Mạng chưa sẵn sàng", "Network not ready")                                          \
    X(SETTINGS_CONV_ENABLING, "Đang bật…", "Enabling…")                                                        \
    X(SETTINGS_CONV_DISABLING, "Đang tắt…", "Disabling…")                                                      \
    X(SETTINGS_CONV_BUSY, "Đang bận, vui lòng chờ", "Busy, please wait")                                                   \
    X(SETTINGS_CONV_CONNECTING, "Đang kết nối…", "Connecting…")                                                    \
    X(SETTINGS_STORAGE_TITLE, "Lưu trữ", "Storage")                                                              \
    X(SETTINGS_STORAGE_PC_BUSY, "SD: máy tính đang dùng", "SD: in use by PC")                                        \
    X(SETTINGS_STORAGE_INSERTED, "SD: đã lắp", "SD: inserted")                                               \
    X(SETTINGS_STORAGE_MISSING, "SD: không phát hiện", "SD: not detected")                                          \
    X(SETTINGS_STORAGE_CAP_DASH, "Dung lượng: —", "Capacity: —")                                                       \
    X(SETTINGS_STORAGE_CAP_EXPORT, "Dung lượng: đang chia sẻ; tắt để xem", "Capacity: exporting; disable to view")         \
    X(SETTINGS_STORAGE_CAP_FMT, "Dung lượng: %.1fGB/%.1fGB", "Capacity: %.1fGB/%.1fGB")                                 \
    X(SETTINGS_STORAGE_CAP_FAIL, "Dung lượng: đọc thất bại", "Capacity: read failed")                                      \
    X(SETTINGS_STORAGE_USB_OFF, "Tắt ổ đĩa USB", "Disable USB disk")                                             \
    X(SETTINGS_STORAGE_USB_ON, "Bật ổ đĩa USB", "Enable USB disk")                                               \
    X(SETTINGS_STORAGE_USB_DISABLED, "Firmware này chưa bật ổ đĩa USB", "USB disk not enabled in this firmware")         \
    X(SETTINGS_STORAGE_USB_HINT_IDLE, "Sau khi bật, máy tính sẽ thấy thiết bị này như một ổ USB", "After enabling, a PC can see this device as a USB drive") \
    X(SETTINGS_STORAGE_USB_HINT_SWITCHING, "Đang chuyển, vui lòng chờ…", "Switching, please wait…")                       \
    X(SETTINGS_STORAGE_USB_HINT_ENABLING, "Đang bật ổ đĩa USB…", "Enabling USB disk…")                            \
    X(SETTINGS_STORAGE_USB_HINT_DISABLING, "Đang tắt ổ đĩa USB…", "Disabling USB disk…")                          \
    X(SETTINGS_STORAGE_USB_HINT_HOST, "Đã bật, hãy mở trên máy tính\nTháo ổ an toàn trước khi tắt", "Enabled, open it on the PC\nEject before turning it off") \
    X(SETTINGS_STORAGE_USB_HINT_LOCAL, "Đã bật — máy tính chưa dùng; tắt để khôi phục gỡ lỗi USB", "Enabled — host idle; disable to restore USB debug") \
    X(SETTINGS_STORAGE_USB_HINT_ENABLE_FAIL, "Bật thất bại — kiểm tra cáp USB rồi thử lại", "Enable failed — check USB cable and retry") \
    X(SETTINGS_STORAGE_USB_HINT_DISABLE_FAIL, "Tắt thất bại — hãy tháo ổ trên máy tính trước", "Disable failed — eject the disk on PC first") \
    X(SETTINGS_STORAGE_USB_HINT_NO_SD, "Không phát hiện thẻ SD", "No SD card detected")                                  \
    X(SETTINGS_STORAGE_USB_HINT_FORMAT, "Thẻ SD cần định dạng (FAT32)", "SD card needs formatting (FAT32)")           \
    X(SETTINGS_STORAGE_USB_HINT_HOST_BUSY, "Thất bại — hãy tháo ổ trên máy tính trước", "Failed — eject the disk on PC first") \
    X(BT_TITLE, "Bluetooth", "Bluetooth")                                                                           \
    X(BT_RESET_HINT, "Chạm để đặt lại Bluetooth", "Tap to reset Bluetooth")                                              \
    X(BT_VERSION_FMT, "Phiên bản Bluetooth: %s", "Bluetooth version: %s")                                              \
    X(BT_CHECK_VERSION, "Chạm để làm mới", "Tap to refresh") \
    X(BT_CHECKING_VERSION, "Đang kiểm tra", "Checking") \
    X(BT_MODE_TITLE, "Chọn chế độ", "Mode selection")                                                           \
    X(BT_NAME_TITLE, "Tên Bluetooth", "Bluetooth name") \
    X(BT_EDIT_NAME, "Chạm để sửa tên", "Tap to edit name") \
    X(BT_FW_TITLE, "Firmware Bluetooth", "Bluetooth firmware") \
    X(BT_RESET_TITLE, "Đặt lại Bluetooth", "Bluetooth reset") \
    X(BT_RESET_DETAIL, "Khôi phục chế độ hiện tại sau khi đặt lại", "Restore current mode after reset") \
    X(BT_RESET_ACTION, "Chạm để đặt lại chip Bluetooth", "Tap to reset Bluetooth chip") \
    X(BT_CURRENT_MODE, "Chế độ hiện tại", "Current mode") \
    X(BT_CURRENT_MODE_FMT, "Chế độ %u · %s", "Mode %u · %s") \
    X(BT_MODE1_DESC, "Dùng micro và loa tích hợp", "Use the built-in microphone and speaker") \
    X(BT_MODE2_DESC, "Phát âm thanh qua loa hoặc tai nghe Bluetooth", "Play audio through a Bluetooth speaker or headset") \
    X(BT_MODE3_DESC, "Nhận âm thanh phát từ điện thoại hoặc thiết bị khác", "Receive audio played by a phone or other device") \
    X(BT_MODE5_DESC, "Gửi phím bấm và giọng nói tới thiết bị đã kết nối", "Send keystrokes and voice to the connected device") \
    X(BT_NAME_EDIT_HINT, "Nhập tên mới rồi chạm dấu tích để xem lại thay đổi.", "Enter a new name, then tap the checkmark to review the change.") \
    X(BT_NAME_CONFIRM_FMT, "Đổi tên Bluetooth thành “%s”?\nMô-đun sẽ khởi động lại (~700 ms) và ngắt kết nối.", "Change Bluetooth name to “%s”?\nThe module will restart (~700 ms) and disconnect.") \
    X(BT_NAME_CONFIRM, "Xác nhận đổi", "Confirm change") \
    X(BT_NAME_RESTARTING, "Đã gửi tên; Bluetooth đang khởi động lại", "Name sent; Bluetooth restarting") \
    X(BT_NAME_FAILED, "Đổi tên thất bại; hãy thử lại", "Name operation failed; try again") \
    X(BT_NAME_INVALID, "Dùng 1–63 byte, không có ký tự điều khiển", "Use 1–63 bytes without control characters") \
    X(BT_RESET_BTN, "Đặt lại BT", "Reset BT")                                                                    \
    X(BT_MODE1, "Âm thanh trên máy", "Local audio")                                                                             \
    X(BT_MODE2, "Phát qua Bluetooth", "Bluetooth transmit")                                                                             \
    X(BT_MODE3, "Nhận qua Bluetooth", "Bluetooth receive")                                                                             \
    X(BT_MODE5, "Bàn phím & giọng nói Bluetooth", "Bluetooth keyboard & voice") \
    X(BT_MODE5_SET, "Chế độ 5: ghép đôi trên máy tính và chọn micro Bluetooth", "Mode 5: pair on PC and select Bluetooth mic") \
    X(BT_SWITCH_MODE5, "Đang chuyển sang chế độ 5...", "Switching to mode 5...") \
    X(BT_HID_TITLE, "Thử phím tắt HID", "HID shortcut test") \
    X(BT_AUDIO_DEVICES, "Quản lý thiết bị Bluetooth", "Bluetooth device manager") \
    X(BT_HID_HINT, "Ghép đôi HID trên máy tính rồi thử trong một ô văn bản trống. Mỗi lần bấm gửi một phím; giữ Alt để thử phím giữ.", "Pair HID on your PC, then test in a blank text field. Click buttons send one keystroke; hold Alt to test a held key.") \
    X(BT_HID_ENTER, "Enter", "Enter") \
    X(BT_HID_LEFT, "Trái", "Left") \
    X(BT_HID_RIGHT, "Phải", "Right") \
    X(BT_HID_RELEASE, "Nhả tất cả", "Release all") \
    X(BT_HID_HOLD_ALT, "Giữ Alt", "Hold Alt") \
    X(BT_HID_SPACE, "Space", "Space") \
    X(BT_HID_TAB, "Tab", "Tab") \
    X(BT_HID_ESC, "Esc", "Esc") \
    X(BT_HID_BACKSPACE, "Backspace", "Backspace") \
    X(BT_HID_DELETE, "Delete", "Delete") \
    X(BT_HID_UP, "Lên", "Up") \
    X(BT_HID_DOWN, "Xuống", "Down") \
    X(BT_HID_READY, "Kiểm tra phím bấm trên máy tính", "Check keystrokes on your PC") \
    X(BT_HID_WAITING, "Đã gửi; đang chờ Bluetooth phản hồi", "Sent; waiting for Bluetooth response") \
    X(BT_HID_SUBMITTED, "Đã gửi yêu cầu phím; kiểm tra trên máy tính", "Key request submitted; check the PC") \
    X(BT_HID_INVALID, "Tham số phím tắt không hợp lệ", "Invalid shortcut parameters") \
    X(BT_HID_DISCONNECTED, "HID mất kết nối; hãy ghép đôi và kết nối trên máy tính", "HID disconnected; pair and connect on PC") \
    X(BT_HID_SEND_FAILED, "Không gửi được phím tắt; hãy thử lại", "Could not send shortcut; try again") \
    X(BT_HID_NEED_MODE5, "Hãy chuyển sang chế độ 5 trước", "Switch to mode 5 first") \
    X(BT_SELECT_MODE, "Chọn chế độ Bluetooth", "Select a Bluetooth mode")                                              \
    X(BT_MODE1_ACTIVE, "Đang ở chế độ 1\n(AT+RX=2 / AT+MODE=1)", "Mode 1 active\n(AT+RX=2 / AT+MODE=1)")             \
    X(BT_SCAN_BTN, "Quét", "Scan")                                                                         \
    X(BT_MUSIC_BTN, "Nhạc", "Music")                                                                       \
    X(BT_CALL_BTN, "Gọi", "Call")                                                                         \
    X(BT_CONNECTING_FMT, "Đang kết nối: %s...", "Connecting: %s...")                                                  \
    X(BT_MODE1_SET, "Đã đặt chế độ 1", "Mode 1 set")                                                               \
    X(BT_MODE2_SET, "Đã đặt chế độ 2, sẵn sàng quét", "Mode 2 set, ready to scan")                                     \
    X(BT_MODE3_SET, "Đã đặt chế độ 3", "Mode 3 set")                                                               \
    X(BT_SCANNING, "Đang quét...", "Scanning...")                                                               \
    X(BT_FOUND_FMT, "Tìm thấy: %s", "Found: %s")                                                                \
    X(BT_SCAN_DONE_FMT, "Quét xong, %d thiết bị", "Scan done, %d device(s)")                                     \
    X(BT_CONNECTING, "Đang kết nối...", "Connecting...")                                                           \
    X(BT_CONNECT_OK, "Đã kết nối", "Connected")                                                                  \
    X(BT_CONNECT_TIMEOUT, "Kết nối thất bại (hết thời gian)", "Connect failed (timeout)")                                        \
    X(BT_CALL_MODE_SCO, "Chế độ gọi (SCO bật)", "Call mode (SCO up)")                                          \
    X(BT_MUSIC_MODE_SCO, "Chế độ nhạc (SCO tắt)", "Music mode (SCO down)")                                      \
    X(BT_SWITCH_MODE1, "Đang chuyển sang chế độ 1...", "Switching to mode 1...")                                               \
    X(BT_SWITCH_MODE2, "Đang chuyển sang chế độ 2...", "Switching to mode 2...")                                               \
    X(BT_SWITCH_MODE3, "Đang chuyển sang chế độ 3...", "Switching to mode 3...")                                               \
    X(BT_UART_NOT_INIT, "UART chưa khởi tạo", "UART not initialized")                                               \
    X(BT_SWITCH_CALL, "Đang chuyển sang chế độ gọi...", "Switching to call mode...")                                          \
    X(BT_SWITCH_MUSIC, "Đang chuyển sang chế độ nhạc...", "Switching to music mode...")                                        \
    X(BT_PWR_RESET_OK, "Đã đặt lại nguồn BT", "BT power reset")                                                      \
    X(BT_PWR_RESETTING, "Đang đặt lại, vui lòng chờ...", "Resetting, please wait...")                                     \
    X(BT_PWR_RESET_RECONNECT, "Đặt lại xong, đang kết nối lại...", "Reset complete, reconnecting...")                    \
    X(BT_PWR_RESET_FAILED, "Đặt lại thất bại, vui lòng thử lại", "Reset failed, please retry")                                   \
    X(BT_NEED_MODE2, "Hãy chuyển sang chế độ 2 trước", "Switch to mode 2 first")                                               \
    X(BT_SCAN_START, "Đang bắt đầu quét...", "Starting scan...")                                                        \
    X(BT_NEED_CONNECT, "Hãy kết nối thiết bị Bluetooth trước", "Connect a Bluetooth device first")                                  \
    X(SETTINGS_FRONTLIGHT_TITLE, "Đèn nền", "Front light")                                                        \
    X(SETTINGS_FRONTLIGHT_HINT, "Nhiệt độ màu và độ sáng 0–100", "CCT and brightness 0–100")                                \
    X(SETTINGS_FRONTLIGHT_COOL, "Lạnh", "Cool")                                                                \
    X(SETTINGS_FRONTLIGHT_WARM, "Ấm", "Warm")                                                                \
    X(SETTINGS_FRONTLIGHT_BOTH, "Lạnh + ấm", "Cool + warm")                                                       \
    X(SETTINGS_FRONTLIGHT_OFF, "Tắt", "Off")                                                                  \
    X(SETTINGS_FRONTLIGHT_CURRENT_COOL, "Hiện tại: Lạnh", "Current: Cool")                                         \
    X(SETTINGS_FRONTLIGHT_CURRENT_WARM, "Hiện tại: Ấm", "Current: Warm")                                         \
    X(SETTINGS_FRONTLIGHT_CURRENT_BOTH, "Hiện tại: Lạnh + ấm", "Current: Cool + warm")                                \
    X(SETTINGS_FRONTLIGHT_CURRENT_OFF, "Hiện tại: Tắt", "Current: Off")                                           \
    X(SETTINGS_FRONTLIGHT_BRIGHTNESS_FMT, "Độ sáng: %d", "Brightness: %d")                                        \
    X(SETTINGS_FRONTLIGHT_BRIGHTNESS_DEC, "-", "-")                                                            \
    X(SETTINGS_FRONTLIGHT_BRIGHTNESS_INC, "+", "+")                                                            \
    X(SETTINGS_ABOUT_MODEL, "Kiểu máy", "Device model")                                                        \
    X(SETTINGS_ABOUT_CHIP, "Chip", "Chip")                                                                 \
    X(SETTINGS_ABOUT_CORES, "Số nhân CPU", "CPU cores")                                                           \
    X(SETTINGS_ABOUT_CORES_FMT, "%u nhân", "%u cores")                                                           \
    X(OTA_CHECK_FAILED, "Kiểm tra thất bại. Kiểm tra mạng rồi thử lại.", "Check failed. Check network and retry.") \
    X(OTA_ALREADY_LATEST, "Firmware đã là bản mới nhất", "Firmware is up to date") \
    X(SETTINGS_ABOUT_FW, "Kiểm tra cập nhật", "Check for updates")                                                               \
    X(SETTINGS_ABOUT_FW_VER_FMT, "Firmware %s", "Firmware %s")                                                 \
    X(SETTINGS_ABOUT_BUILD, "Thời gian build", "Build time")                                                          \
    X(SETTINGS_ABOUT_MAC, "MAC", "MAC")                                                                   \
    X(SETTINGS_ABOUT_FLASH, "Flash", "Flash")                                                             \
    X(SETTINGS_ABOUT_PSRAM, "PSRAM", "PSRAM")                                                           \
    X(SETTINGS_ABOUT_NONE, "Không có", "None")                                                                       \
    X(SETTINGS_TEST_AUTO, "Kiểm tra tự động", "Auto test")                                                             \
    X(SETTINGS_TEST_TOUCH, "Kiểm tra cảm ứng", "Touch test")                                                           \
    X(SETTINGS_TEST_BATTERY, "Kiểm tra pin", "Battery test")                                                       \
    X(SETTINGS_TEST_AGING, "Kiểm tra lão hóa", "Aging test")                                                           \
    X(SETTINGS_TEST_SIGNAL_SKIP, "Tín hiệu: lượt này chưa nối WiFi/4G", "Signal: WiFi/4G not wired this round")          \
    X(SETTINGS_TEST_OK, "OK", "OK")                                                                          \
    X(SETTINGS_TEST_FAIL, "Thất bại", "Failed")                                                                    \
    X(SETTINGS_TEST_SKIP, "Bỏ qua", "Skip")                                                                      \
    X(SETTINGS_TEST_NOT_DETECTED, "Không tìm thấy", "Not found")                                                     \
    X(SETTINGS_TEST_READ_FAIL, "Đọc thất bại", "Read failed")                                                      \
    X(SETTINGS_TEST_SDCARD, "Thẻ SD", "SD card")                                                                 \
    X(SETTINGS_TEST_SD_MOUNT_FAIL, "Không có thẻ / gắn thất bại", "No card / mount fail")                                  \
    X(SETTINGS_TEST_AUDIO_TITLE, "Âm thanh", "Audio")                                                              \
    X(SETTINGS_TEST_AUDIO_HOLD, "Giữ để nói", "Hold to talk")                                                    \
    X(SETTINGS_TEST_AUDIO_RECORDING, "Đang ghi âm…", "Recording…")                                                  \
    X(SETTINGS_TEST_AUDIO_CONFIRM, "Micro và loa hoạt động tốt?", "Mic and speaker OK?")                               \
    X(SETTINGS_TEST_YES, "Có", "Yes")                                                                          \
    X(SETTINGS_TEST_NO, "Không", "No")                                                                            \
    X(RECORD_PLAYING, "Đang phát…", "Playing…")                                                                   \
    X(SETTINGS_TEST_CAMERA, "Camera", "Camera")                                                                \
    X(SETTINGS_TEST_BACK, "Quay lại", "Back")                                                                      \
    X(SETTINGS_TEST_TOUCH_START, "Chạm màn hình để bắt đầu", "Tap screen to start")                                  \
    X(SETTINGS_TEST_BATTERY_HINT, "SOC: 3.30~4.30V; ≥4.30 giữ 30s→100%",                                         \
      "SOC: 3.30~4.30V; ≥4.30 hold 30s→100%")                                                                   \
    X(SETTINGS_TEST_BATTERY_GAUGE, "Bộ đo pin", "Fuel gauge")                                                     \
    X(SETTINGS_TEST_BATTERY_SOC_SMOOTH, "SOC (làm mượt)", "SOC (smooth)")                                          \
    X(SETTINGS_TEST_BATTERY_SOC_RAW, "SOC (tức thời)", "SOC (instant)")                                            \
    X(SETTINGS_TEST_BATTERY_VOLT, "Điện áp", "Voltage")                                                           \
    X(SETTINGS_TEST_BATTERY_VOLT_RANGE, "Dao động điện áp", "Voltage swing")                                           \
    X(SETTINGS_TEST_BATTERY_CURR, "Dòng điện", "Current")                                                           \
    X(SETTINGS_TEST_BATTERY_GAUGE_STAT, "Trạng thái bộ đo", "Gauge state")                                             \
    X(SETTINGS_TEST_BATTERY_CHARGING, "Đang sạc", "Charging")                                                    \
    X(SETTINGS_TEST_BATTERY_DISCHARGING, "Đang xả", "Discharging")                                              \
    X(SETTINGS_TEST_BATTERY_IDLE_LOAD, "Rảnh", "Idle")                                                         \
    X(SETTINGS_TEST_BATTERY_VBUS, "VBUS", "VBUS")                                                              \
    X(SETTINGS_TEST_BATTERY_CHIP_CHG, "Chip sạc", "Chip charge")                                               \
    X(SETTINGS_TEST_BATTERY_CHG_EN, "Cho phép sạc", "Charge enable")                                               \
    X(SETTINGS_TEST_BATTERY_ICHG, "Dòng sạc", "Charge current")                                                \
    X(SETTINGS_TEST_BATTERY_VREG, "Điện áp sạc", "Charge voltage")                                                \
    X(SETTINGS_TEST_BATTERY_MATCH, "Nhất quán", "Consistency")                                                    \
    X(SETTINGS_TEST_BATTERY_CHG_NOT, "Không sạc / đã đầy", "Not charging / full")                                     \
    X(SETTINGS_TEST_BATTERY_CHG_CC, "Nhỏ giọt/trước/CC", "Trickle/pre/CC")                                          \
    X(SETTINGS_TEST_BATTERY_CHG_CV, "Giảm dòng CV", "CV taper")                                                    \
    X(SETTINGS_TEST_BATTERY_CHG_TOPOFF, "Sạc bù", "Top-off")                                                  \
    X(SETTINGS_TEST_BATTERY_EN_ON, "Bật", "On")                                                                 \
    X(SETTINGS_TEST_BATTERY_EN_OFF, "Tắt", "Off")                                                               \
    X(SETTINGS_TEST_BATTERY_VBUS_NONE, "Không có nguồn vào", "No input")                                                   \
    X(SETTINGS_TEST_BATTERY_VBUS_SDP, "USB SDP", "USB SDP")                                                    \
    X(SETTINGS_TEST_BATTERY_VBUS_CDP, "USB CDP", "USB CDP")                                                    \
    X(SETTINGS_TEST_BATTERY_VBUS_DCP, "USB DCP", "USB DCP")                                                    \
    X(SETTINGS_TEST_BATTERY_VBUS_UNK_ADP, "Bộ sạc không rõ", "Unknown adapter")                                     \
    X(SETTINGS_TEST_BATTERY_VBUS_NONSTD, "Bộ sạc không chuẩn", "Non-std adapter")                                      \
    X(SETTINGS_TEST_BATTERY_VBUS_OTG, "OTG", "OTG")                                                            \
    X(SETTINGS_TEST_BATTERY_VBUS_ADP, "Bộ sạc", "Adapter")                                                     \
    X(SETTINGS_TEST_BATTERY_VBUS_UNKNOWN_IN, "Nguồn vào (không rõ)", "Input (unknown)")                              \
    X(SETTINGS_TEST_BATTERY_MATCH_OK, "Khớp", "Match")                                                         \
    X(SETTINGS_TEST_BATTERY_MATCH_DPDM, "Khớp (DPDM tắt)", "Match (DPDM off)")                                    \
    X(SETTINGS_TEST_BATTERY_MISMATCH_CHIP, "Chip sạc / bộ đo không", "Chip chg / gauge no")                           \
    X(SETTINGS_TEST_BATTERY_MISMATCH_GAUGE, "Bộ đo sạc / chip không", "Gauge chg / chip no")                          \
    X(SETTINGS_TEST_BATTERY_MISMATCH_NO_CHG, "Có nguồn, không sạc", "Input, not charging")                            \
    X(SETTINGS_TEST_BATTERY_MISMATCH_NO_IN, "Không có nguồn mà đang sạc?", "No input but charging?")                         \
    X(SETTINGS_TEST_AGING_NO_WALLPAPER, "Không có hình nền", "No wallpaper") \
    X(SETTINGS_TEST_AGING_SD_NOT_READY, "Chưa gắn thẻ SD.", "SD card not mounted.") \
    X(SETTINGS_TEST_AGING_SD_INIT_TIMEOUT, "Khởi tạo SD quá lâu. Kiểm tra thẻ và tiếp điểm.", "SD init timed out. Check card and contacts.") \
    X(SETTINGS_TEST_AGING_SD_MOUNT_FAILED, "Gắn SD thất bại. Kiểm tra hệ thống tệp.", "SD mount failed. Check the file system.") \
    X(SETTINGS_TEST_AGING_SD_INIT_MEMORY, "Không đủ bộ nhớ để khởi tạo SD. Hãy khởi động lại.", "Not enough memory to initialize SD. Restart.") \
    X(SETTINGS_TEST_AGING_SD_INIT_FAILED, "Khởi tạo SD thất bại. Kiểm tra thẻ.", "SD initialization failed. Check the card.") \
    X(SETTINGS_TEST_AGING_SD_USB, "Máy tính đang dùng SD. Tắt chế độ ổ USB.", "SD in use by PC. Turn off USB drive mode.") \
    X(SETTINGS_TEST_AGING_SD_BUSY, "Không truy cập được SD. Đang thử lại.", "SD access unavailable. Retrying.") \
    X(SETTINGS_TEST_AGING_SD_DIR_MISSING, "Thiếu thư mục hình nền. Kiểm tra thẻ SD.", "Wallpaper folder missing. Check SD card.") \
    X(SETTINGS_TEST_AGING_SD_DIR_OPEN, "Không mở được thư mục hình nền. Kiểm tra thẻ SD.", "Cannot open wallpaper folder. Check SD card.") \
    X(SETTINGS_TEST_AGING_SD_DIR_READ, "Không đọc được thư mục hình nền. Kiểm tra thẻ SD.", "Cannot read wallpaper folder. Check SD card.") \
    X(SETTINGS_TEST_AGING_SD_FILE_STAT, "Không đọc được thông tin tệp. Kiểm tra thẻ SD.", "Cannot read file info. Check SD card.") \
    X(SETTINGS_TEST_AGING_SD_FILE_READ, "Không đọc được tệp hình nền. Kiểm tra thẻ SD.", "Cannot read wallpaper file. Check SD card.") \
    X(SETTINGS_TEST_AGING_SD_IMAGE_INVALID, "Ảnh hỏng hoặc thiếu. Hãy thay hình nền.", "Image damaged or incomplete. Replace wallpaper.") \
    X(SETTINGS_TEST_AGING_SD_IMAGE_UNSUPPORTED, "Định dạng hoặc kích thước không hỗ trợ. Hãy thay hình nền.", "Format or size unsupported. Replace wallpaper.") \
    X(SETTINGS_TEST_AGING_SD_DECODE, "Không giải mã được hình nền. Thay ảnh rồi thử lại.", "Cannot decode wallpaper. Replace image, retry.") \
    X(SETTINGS_TEST_AGING_SD_MEMORY, "Không đủ bộ nhớ để giải mã. Đang thử lại.", "Not enough memory to decode. Retrying.") \
    X(SETTINGS_TEST_AGING_SD_TASK, "Không bắt đầu được tác vụ đọc hình nền. Đang thử lại.", "Cannot start wallpaper read task. Retrying.") \
    X(SETTINGS_TEST_AGING_RESTART_COUNT_FMT, "Số lần khởi động lại: %d", "Restarts: %d") \
    X(SETTINGS_TEST_AGING_SAVE_FAILED, "Không lưu được trạng thái lão hóa. Đang thử lại.", "Cannot save aging state. Retrying.") \
    X(SETTINGS_TEST_AGING_RUNNING, "Đang chạy lão hóa", "Aging in progress")                              \
    X(SETTINGS_TEST_AGING_TICK_FMT, "Đã chạy %u giây  rung=%d", "Running %u s  vibe=%d")                               \
    X(STANDBY, "Chờ", "Standby")                                                                              \
    X(CONNECTING, "Đang kết nối...", "Connecting...")                                                                \
    X(LISTENING, "Đang nghe...", "Listening...")                                                                  \
    X(SPEAKING, "Đang nói...", "Speaking...")                                                                    \
    X(PLEASE_WAIT, "Vui lòng chờ...", "Please wait...")                                                              \
    X(LOADING_PROTOCOL, "Đang đăng nhập...", "Logging in...")                                                        \
    X(VOICE_STARTING_NET, "Đang kết nối; sẽ tự bắt đầu", "Connecting; will start automatically")           \
    X(SERVER_NOT_CONNECTED, "Không kết nối được dịch vụ", "Unable to connect to service")                      \
    X(WIFI_CONFIG_MODE, "Cài đặt Wi-Fi", "Wi-Fi Setup")                                                             \
    X(NEED_WIFI_CFG, "Hãy cài đặt Wi-Fi trước", "Set up Wi-Fi first")                                                     \
    X(VERSION, "Phiên bản ", "Ver ")                                                                                \
    X(FOUND_NEW_ASSETS, "Có tài nguyên mới: %s", "Found new assets: %s")                                              \
    X(LOADING_ASSETS, "Đang tải tài nguyên...", "Loading assets...")                                                      \
    X(ERROR, "Lỗi", "Error")                                                                                  \
    X(DOWNLOAD_ASSETS_FAILED, "Tải tài nguyên thất bại", "Failed to download assets")                                     \
    X(CHECKING_NEW_VERSION, "Đang kiểm tra phiên bản mới...", "Checking for new version...")                                    \
    X(CHECK_NEW_VERSION_FAILED, "Kiểm tra phiên bản thất bại, thử lại sau %d giây: %s", "Check version failed, retry in %d s: %s") \
    X(ACTIVATION, "Kích hoạt", "Activation")                                                                    \
    X(ACTIVATION_CODE_FMT, "Mã:%s", "Code:%s")                                                             \
    X(OTA_UPGRADE, "Nâng cấp OTA", "OTA Upgrade")                                                                  \
    X(UPGRADING, "Hệ thống đang nâng cấp...", "System is upgrading...")                                                  \
    X(UPGRADE_FAILED, "Nâng cấp thất bại", "Upgrade failed")                                                            \
    X(OTA_SUCCESS_REBOOT, "Nâng cấp xong, đang khởi động lại", "Upgrade OK, rebooting")                                       \
    X(OTA_MANUAL, "Nâng cấp thủ công", "Manual upgrade")                                                                \
    X(RTC_MODE_OFF, "Tắt AEC", "AEC Off")                                                                     \
    X(RTC_MODE_ON, "Bật AEC", "AEC On")                                                                       \
    X(SERVER_ERROR, "Lỗi máy chủ", "Server error")                                                                \
    X(SERVER_TIMEOUT, "Máy chủ quá thời gian", "Server timeout")                                                            \
    X(OTA_TITLE, "Nâng cấp hệ thống", "System upgrade")                                                                 \
    X(OTA_CUR_VER_FMT, "Hiện tại: %s", "Current: %s")                                                          \
    X(OTA_NEW_VER_FMT, "Mới: %s", "New: %s")                                                                \
    X(OTA_HINT, "Không tắt nguồn khi đang nâng cấp", "Do not power off during upgrade")                                         \
    X(OTA_CONFIRM_FMT, "Phiên bản mới %s\nNâng cấp?", "New version %s\nUpgrade?")                                 \
    X(OTA_SD_CONFIRM_FMT, "Firmware trên SD\nHiện tại %s\nNâng cấp?", "SD firmware\nCurrent %s\nUpgrade?")              \
    X(OTA_UPGRADE_NOW, "Nâng cấp ngay", "Upgrade now")                                                              \
    X(OTA_IGNORE_VERSION, "Bỏ qua phiên bản", "Ignore version")                                                      \
    X(OTA_REMIND_LATER, "Nhắc sau", "Remind later")                                                            \
    X(COMMON_CANCEL, "Hủy", "Cancel")                                                                         \
    X(COMMON_REMOVE, "Xóa", "Remove")                                                                         \
    X(COMMON_SELECT_ALL, "Chọn tất cả", "Select all")                                                                 \
    X(TASK_API_ERROR, "Lỗi API", "API error")                                                             \
    X(TASK_BATCH_COMPLETING, "Đang hoàn thành hàng loạt...", "Batch completing...")                                           \
    X(TASK_BATCH_FAIL, "Xử lý hàng loạt thất bại", "Batch failed")                                                         \
    X(TASK_BATCH_REMOVING, "Đang xóa hàng loạt...", "Batch removing...")                                               \
    X(TASK_BATCH_RESULT_FMT, "%d thành công, %d thất bại", "%d ok, %d failed")                                            \
    X(TASK_BATCH_START_FAIL, "Không bắt đầu được xử lý hàng loạt", "Failed to start batch")                                      \
    X(TASK_CHOOSE_ACTION, "Chọn thao tác", "Choose action")                                                         \
    X(TASK_COMPLETE, "Xong", "Done")                                                                           \
    X(TASK_COMPLETED, "Đã hoàn thành", "Completed")                                                                   \
    X(TASK_COMPLETED_N_FMT, "Đã hoàn thành %d", "Completed %d")                                                    \
    X(TASK_COMPLETE_FAIL, "Hoàn thành thất bại", "Complete failed")                                                       \
    X(TASK_COMPLETE_START_FAIL, "Không bắt đầu được việc hoàn thành", "Failed to start complete")                                \
    X(TASK_COMPLETE_TODO, "Hoàn thành", "Complete")                                                              \
    X(TASK_COMPLETING, "Đang hoàn thành...", "Completing...")                                                           \
    X(TASK_CONNECTING_NET, "Đang kết nối…", "Connecting…")                                                         \
    X(TASK_CONN_FAIL, "Kết nối thất bại", "Connection failed")                                                     \
    X(TASK_DATA_FORMAT_ERR, "Sai định dạng dữ liệu", "Bad data format")                                                 \
    X(TASK_DELETED, "Đã xóa", "Deleted")                                                                       \
    X(TASK_DELETE_FAIL, "Xóa thất bại", "Delete failed")                                                           \
    X(TASK_DELETE_START_FAIL, "Không bắt đầu được việc xóa", "Failed to start delete")                                    \
    X(TASK_DELETE_TODO, "Xóa", "Delete")                                                                  \
    X(TASK_DELETING, "Đang xóa...", "Deleting...")                                                               \
    X(TASK_DONE_OK, "Đã hoàn thành công việc", "Task completed")                                                            \
    X(TASK_EMPTY_DONE, "Chưa có công việc đã hoàn thành", "No completed tasks")                                                 \
    X(TASK_EMPTY_TODO, "Không có việc cần làm", "No todos")                                                                 \
    X(TASK_INVALID, "Việc cần làm không hợp lệ", "Invalid todo")                                                                \
    X(TASK_LOAD_FAIL, "Tải thất bại", "Load failed")                                                               \
    X(TASK_NEED_WIFI_CFG, "Hãy cài đặt Wi-Fi trước", "Set up Wi-Fi first")                                                 \
    X(TASK_NET_NOT_READY, "Mạng chưa sẵn sàng", "Network not ready")                                                   \
    X(TASK_NO_COMPLETABLE, "Không có gì để hoàn thành", "Nothing to complete")                                          \
    X(TASK_NO_NETWORK, "Không có mạng", "No network")                                                                 \
    X(TASK_PARSE_FAIL, "Đọc phản hồi thất bại", "Response parse failed")                                                \
    X(TASK_PLEASE_WAIT, "Vui lòng chờ...", "Please wait...")                                                         \
    X(TASK_REFRESH, "Làm mới", "Refresh")                                                                         \
    X(TASK_REFRESHED, "Đã làm mới", "Refreshed")                                                                   \
    X(TASK_REFRESHING, "Đang làm mới...", "Refreshing...")                                                           \
    X(TASK_REFRESH_START_FAIL, "Không bắt đầu được việc làm mới", "Failed to start refresh")                                      \
    X(TASK_REMOVED_N_FMT, "Đã xóa %d", "Removed %d")                                                        \
    X(TASK_REQUEST_FAIL, "Yêu cầu thất bại", "Request failed")                                                         \
    X(TASK_SELECTED_FMT, "Đã chọn %d", "Selected %d")                                                             \
    X(TASK_SELECT_FIRST, "Hãy chọn mục trước", "Select items first")                                                     \
    X(TASK_TAB_DONE, "Đã xong", "Done")                                                                         \
    X(TASK_TAB_TODO, "Cần làm", "Todo")                                                                           \
    X(TASK_TITLE_DONE, "Đã xong", "Done")                                                                       \
    X(TASK_TITLE_TODO, "Lịch trình", "Schedule")                                                                 \
    X(CONNECT_TO, "Kết nối ", "Connect ")                                                                         \
    X(CONNECTED_TO, "Đã kết nối ", "Connected ")                                                                   \
    X(SETTINGS_NET_WIFI_CFG_TITLE, "Mạng Wi-Fi", "Wi-Fi network")                                               \
    X(SETTINGS_NET_WIFI_CFG_HINT, "Quét Wi-Fi gần đây và nhập mật khẩu", "Scan nearby Wi-Fi and enter the password") \
    X(SETTINGS_NET_ENTER_CFG, "Cấu hình Wi-Fi", "Configure Wi-Fi")                                             \
    X(SETTINGS_NET_REBOOT_COUNT_FMT, "%s\nKhởi động lại sau %d giây…", "%s\nRebooting in %d s…")                             \
    X(SETTINGS_NET_REBOOTING, "Đang khởi động lại…", "Rebooting…")                                                       \
    X(NETWORK_TITLE, "Cấu hình Wi-Fi", "Configure Wi-Fi")                                                           \
    X(NETWORK_TAB_NEARBY, "Gần đây", "Nearby")                                                                    \
    X(NETWORK_TAB_SAVED, "Đã lưu", "Saved")                                                                    \
    X(NETWORK_SCAN, "Làm mới", "Refresh")                                                                         \
    X(NETWORK_SCANNING, "Đang quét Wi-Fi gần đây…", "Scanning nearby Wi-Fi…")                                        \
    X(NETWORK_WIFI_INIT, "Đang khởi tạo Wi-Fi…", "Initializing Wi-Fi…")                                            \
    X(NETWORK_WIFI_INIT_FAIL, "Khởi tạo Wi-Fi thất bại", "Wi-Fi init failed")                                          \
    X(NETWORK_SCAN_FAIL, "Không bắt đầu quét được", "Failed to start scan")                                               \
    X(NETWORK_SCAN_TIMEOUT, "Quét quá thời gian", "Scan timed out")                                                      \
    X(NETWORK_SCAN_DONE_FMT, "Quét xong, %d mạng", "Scan done, %d networks")                               \
    X(NETWORK_SCAN_TASK_FAIL, "Không bắt đầu được tác vụ quét", "Cannot start scan task")                                    \
    X(NETWORK_BUSY_CONNECT, "Đang kết nối; hãy quét sau", "Connecting; scan later")                                \
    X(NETWORK_NEARBY_EMPTY, "Không có mạng. Chạm Làm mới để thử lại", "No networks. Tap Refresh to retry")                  \
    X(NETWORK_SAVED_EMPTY, "Chưa lưu Wi-Fi nào", "No saved Wi-Fi yet")                                           \
    X(NETWORK_AUTH_OPEN, "[Mở]", "[Open]")                                                                   \
    X(NETWORK_AUTH_SECURE, "[Bảo mật]", "[Secured]")                                                              \
    X(NETWORK_CONNECT_TO_FMT, "Kết nối tới: %s", "Connect to: %s")                                                  \
    X(NETWORK_PWD_HINT, "Nhập mật khẩu Wi-Fi (8–63 ký tự)", "Enter Wi-Fi password (8–63 chars)")                   \
    X(NETWORK_PWD_PLACEHOLDER, "Mật khẩu Wi-Fi", "Wi-Fi password")                                                  \
    X(NETWORK_SHOW_PWD, "Hiện mật khẩu", "Show password")                                                           \
    X(NETWORK_CANCEL, "Hủy", "Cancel")                                                                        \
    X(NETWORK_CONNECT, "Kết nối", "Connect")                                                                      \
    X(NETWORK_CONNECTING_FMT, "Đang kết nối tới %s …", "Connecting to %s …")                                           \
    X(NETWORK_CONNECTED_FMT, "Đã kết nối tới %s", "Connected to %s")                                                   \
    X(NETWORK_CONNECT_FAIL, "Kết nối thất bại", "Connection failed")                                                   \
    X(NETWORK_CONNECT_FAIL_REFRESH, "Hãy làm mới danh sách Wi-Fi", "Please refresh the Wi-Fi list")                   \
    X(NETWORK_CONNECT_TIMEOUT, "Kết nối quá thời gian", "Connection timed out")                                             \
    X(NETWORK_CONNECT_TIMEOUT_HINT, "Không kết nối được trong 15 giây; hãy thử lại", "Could not connect within 15s; retry")   \
    X(NETWORK_CONNECT_TASK_FAIL, "Không bắt đầu được tác vụ kết nối", "Cannot start connect task")                              \
    X(NETWORK_SSID_INVALID, "SSID không hợp lệ", "Invalid SSID")                                                     \
    X(NETWORK_PWD_TOO_LONG, "Mật khẩu quá dài", "Password too long")                                                   \
    X(NETWORK_ERR_BAD_PASSWORD, "Sai mật khẩu; hãy thử lại", "Wrong password; try again")                           \
    X(NETWORK_ERR_AP_GONE, "Không tìm thấy Wi-Fi (mất sóng)", "Wi-Fi not found (signal lost)")                       \
    X(NETWORK_ERR_ASSOC, "Liên kết thất bại; điểm truy cập từ chối", "Association failed; AP rejected")                        \
    X(NETWORK_ERR_WEAK, "Sóng quá yếu; quá thời gian", "Signal too weak; timed out")                                    \
    X(NETWORK_ERR_REASON_FMT, "Bị từ chối (reason=%u)", "Rejected (reason=%u)")                                \
    X(NETWORK_DEFAULT_FMT, "%s (mặc định)", "%s (default)")                                                       \
    X(NETWORK_SET_DEFAULT, "Mặc định", "Default")                                                                  \
    X(NETWORK_DELETE, "Xóa", "Delete")                                                                        \
    X(NETWORK_CLEAR_ALL, "Xóa hết", "Clear")                                                                      \
    X(NETWORK_SET_DEFAULT_OK, "Đã đặt làm mạng mặc định", "Set as default network")                                      \
    X(NETWORK_DELETED_OK, "Đã xóa mạng", "Network deleted")                                                   \
    X(NETWORK_CLEARED_OK, "Đã xóa các mạng đã lưu", "Cleared saved networks")                                        \
    X(SCANNING, "Đang quét...", "Scanning...")                                                                    \
    X(SCANNING_WIFI, "Đang quét Wi-Fi...", "Scanning Wi-Fi...")                                                      \
    X(WIFI_STATUS_SCANNING, "WiFi đang quét", "WiFi scanning")                                                     \
    X(WIFI_STATUS_CONNECTING, "WiFi đang kết nối", "WiFi connecting")                                                 \
    X(VOLUME, "Âm lượng ", "Volume ")                                                                              \
    X(MAX_VOLUME, "Âm lượng tối đa", "Max volume")                                                                    \
    X(MUTED, "Đã tắt tiếng", "Muted")                                                                    \
    X(BOOK_ANTIALIAS, "Khử răng cưa", "Anti-aliasing") \
    X(BOOK_BAD_ARG, "Tham số không hợp lệ", "Invalid argument") \
    X(BOOK_BLANK, "(Trống)", "(Blank)") \
    X(BOOK_BODY, "Nội dung", "Body") \
    X(BOOK_CANCELLED, "Đã hủy", "Cancelled") \
    X(BOOK_CHAPTER_FMT, "Ch.%d", "Ch.%d") \
    X(BOOK_CHAPTER_NO_TEXT, "(Chương không có chữ)", "(No text in chapter)") \
    X(BOOK_CHAPTER_PROGRESS, "Thông tin chân trang", "Footer info") \
    X(BOOK_CHECKSUM_FAIL, "Sai checksum", "Checksum failed") \
    X(BOOK_CONN_FAIL, "Kết nối thất bại", "Connection failed") \
    X(BOOK_CONTINUE, "Đọc tiếp", "Continue") \
    X(BOOK_COUNT_FMT, "%d", "%d") \
    X(BOOK_COVER_TOO_LARGE, "Ảnh bìa quá lớn", "Cover too large") \
    X(BOOK_DELETE_FAILED, "Xóa thất bại", "Delete failed") \
    X(BOOK_DETAIL, "Chi tiết", "Details") \
    X(BOOK_DOWNLOAD_FAIL, "Tải xuống thất bại", "Download failed") \
    X(BOOK_EMPTY_COVER, "Không có bìa", "Empty cover") \
    X(BOOK_EMPTY_FILE, "Tệp trống", "Empty file") \
    X(BOOK_EMPTY_HOME_FMT, "Chưa có sách\nĐặt tệp .epub / .txt / .ebook\nvào %s", "No books\nPut .epub / .txt / .ebook\nin %s") \
    X(BOOK_EMPTY_SHELF_FMT, "Kệ sách trống\nĐặt tệp .epub / .txt / .ebook\nvào %s", "Bookshelf empty\nPut .epub / .txt / .ebook\nin %s") \
    X(BOOK_FILE_TOO_LARGE, "Tệp quá lớn", "File too large") \
    X(BOOK_FINISHED, "Đã đọc xong", "Finished") \
    X(BOOK_FONT_EMPTY, "Không có phông", "No fonts") \
    X(BOOK_FONT_NAME_INVALID, "Tên tệp không hợp lệ", "Invalid filename") \
    X(BOOK_FONT_TITLE, "Phông chữ", "Font") \
    X(BOOK_FOOTER_BATTERY, "Pin", "Battery") \
    X(BOOK_FOOTER_PROGRESS, "Tiến độ", "Progress") \
    X(BOOK_FOOTER_TIME, "Giờ", "Time") \
    X(BOOK_FOOTER_TITLE, "Chương", "Chapter") \
    X(BOOK_IMAGE_PLACEHOLDER, "[Ảnh]", "[Image]") \
    X(BOOK_LAYOUT_BUSY, "Đang dàn trang", "Building") \
    X(BOOK_LAYOUT_DONE, "Xong", "Done") \
    X(BOOK_LINE_GAP, "Giãn dòng", "Line spacing") \
    X(BOOK_MARGIN_TOP, "Trên", "Top") \
    X(BOOK_MARGIN_BOTTOM, "Dưới", "Bottom") \
    X(BOOK_MARGIN_LEFT, "Trái", "Left") \
    X(BOOK_MARGIN_RIGHT, "Phải", "Right") \
    X(BOOK_MARGIN_SIDES, "Hai bên", "Sides") \
    X(BOOK_MARGIN, "Lề", "Margin") \
    X(BOOK_MARGIN_NARROW, "Hẹp", "Narrow") \
    X(BOOK_MARGIN_STANDARD, "Chuẩn", "Standard") \
    X(BOOK_MARGIN_VERY_WIDE, "Rất rộng", "Very wide") \
    X(BOOK_MARGIN_WIDE, "Rộng", "Wide") \
    X(BOOK_ALIGNMENT, "Căn lề", "Alignment") \
    X(BOOK_ALIGN_JUSTIFY, "Đều hai bên", "Justified") \
    X(BOOK_ALIGN_LEFT, "Trái", "Left") \
    X(BOOK_NET_NOT_READY, "Mạng chưa sẵn sàng", "Network not ready") \
    X(BOOK_NO_CONTINUE, "Không có sách đang đọc", "Nothing to continue") \
    X(BOOK_NO_COVER_URL, "Không có URL ảnh bìa", "No cover URL") \
    X(BOOK_NO_DOWNLOAD_URL, "Thiếu URL tải xuống", "Missing download URL") \
    X(BOOK_NO_LOCAL_FILE, "Không có tệp trên máy", "No local file") \
    X(BOOK_NO_NETWORK, "Không có mạng", "No network") \
    X(BOOK_NO_RECENT, "Chưa có sách đọc gần đây", "No recent books") \
    X(BOOK_NO_SD, "Không có thẻ SD\nLắp thẻ rồi thử lại", "No SD card\nInsert and retry") \
    X(BOOK_NO_SD_SHORT, "Không có thẻ SD", "No SD card") \
    X(BOOK_OPEN_FAILED, "Mở thất bại", "Open failed") \
    X(BOOK_OPEN_FAILED_IO, "Mở thất bại\nThiếu tệp hoặc đọc SD lỗi", "Open failed\nFile missing or SD read failed") \
    X(BOOK_OPEN_FAILED_DATA, "Mở thất bại\nSách hỏng hoặc nội dung không hợp lệ", "Open failed\nDamaged book or invalid content") \
    X(BOOK_OPEN_FAILED_MEMORY, "Mở thất bại\nKhông đủ bộ nhớ", "Open failed\nNot enough memory") \
    X(BOOK_OPEN_FAILED_CACHE, "Mở thất bại\nKhông tạo hoặc đọc được bộ đệm. Kiểm tra thẻ SD và dung lượng trống", "Open failed\nCannot create or read the cache. Check the SD card and free space") \
    X(BOOK_OPEN_FAILED_UNSUPPORTED, "Mở thất bại\nĐịnh dạng sách hoặc tham số tệp không được hỗ trợ", "Open failed\nUnsupported book format or file parameters") \
    X(BOOK_OPEN_CANCELLED, "Đã hủy mở sách", "Opening cancelled") \
    X(BOOK_OPEN_FAILED_TASK, "Mở thất bại\nKhông bắt đầu được tác vụ phân tích", "Open failed\nCannot start parse task") \
    X(BOOK_OPENING, "Đang mở", "Opening") \
    X(DIRECTORY_READ_FAILED, "Không đọc được thư mục. Hãy mở lại.", "Cannot read folder. Please reopen it.") \
    X(BOOK_FOLDER, "Thư mục", "Folder") \
    X(BOOK_LOADING, "Đang tải...", "Loading...") \
    X(BOOK_ORIENT, "Hướng", "Orientation") \
    X(BOOK_ORIENT_LAND_LEFT, "Trái", "Left") \
    X(BOOK_ORIENT_LAND_RIGHT, "Phải", "Right") \
    X(BOOK_ORIENT_PORTRAIT, "Dọc", "Port.") \
    X(BOOK_OUT_OF_MEMORY, "Hết bộ nhớ", "Out of memory") \
    X(BOOK_PAGE_LOAD_FAILED, "Không tải được trang", "Cannot load page") \
    X(BOOK_PATH_INVALID, "Đường dẫn không hợp lệ", "Invalid path") \
    X(BOOK_READ_DURATION, "Tổng thời gian", "Total time") \
    X(BOOK_READ_FAIL, "Đọc thất bại", "Read failed") \
    X(BOOK_READ_FILE_FAIL, "(Đọc thất bại)", "(Read failed)") \
    X(BOOK_READ_PCT_FMT, "Đã đọc %d.%d%%", "Read %d.%d%%") \
    X(BOOK_RECENT, "Gần đây", "Recent") \
    X(BOOK_SAVE_FAIL, "Lưu thất bại", "Save failed") \
    X(BOOK_SELECTED_FMT, "Đã chọn %d", "Selected %d") \
    X(BOOK_DELETING, "Đang xóa. Vui lòng chờ thao tác hoàn tất...", "Removing. Please wait for the operation to finish...") \
    X(BOOK_DELETE_STOPPING, "Đang dừng. Vui lòng chờ dọn xong tệp hiện tại...", "Stopping. Please wait for the current file cleanup to finish...") \
    X(BOOK_SHELF_TITLE, "Kệ sách của tôi", "My bookshelf") \
    X(BOOK_SPACING_COMPACT, "Sát", "Compact") \
    X(BOOK_SPACING_RELAXED, "Thoáng", "Relaxed") \
    X(BOOK_SPACING_STANDARD, "Chuẩn", "Standard") \
    X(BOOK_SPACING_VERY_RELAXED, "Rất thoáng", "Very relaxed") \
    X(BOOK_START, "Bắt đầu đọc", "Start reading") \
    X(BOOK_STORAGE_UNAVAIL, "Không dùng được bộ nhớ", "Storage unavailable") \
    X(BOOK_SYNC_EMPTY_BODY, "Nội dung yêu cầu trống", "Empty request body") \
    X(BOOK_TAP_ZONE_BACK, "Quay lại", "Back") \
    X(BOOK_TAP_ZONE_FULL_REFRESH, "Làm mới toàn màn", "Full refresh") \
    X(BOOK_TAP_ZONE_MENU, "Menu", "Menu") \
    X(BOOK_TAP_ZONE_NEXT, "Trang sau", "Next") \
    X(BOOK_TAP_ZONE_NONE, "Không", "None") \
    X(BOOK_TAP_ZONE_PREV, "Trang trước", "Prev") \
    X(BOOK_TAP_ZONES, "Vùng chạm", "Tap zones") \
    X(BOOK_TAP_ZONES_PICK, "Chọn thao tác", "Choose action") \
    X(BOOK_TAP_ZONES_RESET, "Đặt lại", "Reset") \
    X(BOOK_TAP_ZONES_TITLE, "Vùng chạm", "Tap zones") \
    X(BOOK_TOC, "Mục lục", "Contents") \
    X(BOOK_TOC_BUILDING, "Đang tìm mục lục...", "Finding contents...") \
    X(BOOK_TOC_HOME, "Trang chính", "Home") \
    X(BOOK_TOC_EMPTY, "Không có mục lục", "No contents") \
    X(BOOK_DICT_LOOKING_UP, "Đang tra từ...", "Looking up...") \
    X(BOOK_DICT_NOT_FOUND, "Không tìm thấy “%s”", "“%s” not found") \
    X(BOOK_DICT_NONE, "Chưa có từ điển. Chép từ điển vào thư mục /dict trên thẻ SD.", "No dictionary found. Copy dictionaries to /dict on the SD card.") \
    X(BOOK_DICT_SD_UNAVAILABLE, "Thẻ SD đang bận", "SD card unavailable") \
    X(BOOK_TOC_PAGE_FMT, "Mục lục %d/%d", "Contents %d/%d") \
    X(BOOK_TODAY_DURATION, "Hôm nay", "Today") \
    X(BOOK_UNDERLINE, "Gạch chân", "Underline") \
    X(BOOK_UNDERLINE_DASHED, "Nét đứt", "Dashed") \
    X(BOOK_UNDERLINE_SOLID, "Nét liền", "Solid") \
    X(BOOK_WRITE_FAIL, "Ghi thất bại", "Write failed") \
    X(CLOUD_API_ERROR, "Lỗi API", "API error") \
    X(CLOUD_CANCELLED, "Đã hủy", "Cancelled") \
    X(CLOUD_CANCELLING, "Đang hủy…", "Cancelling…") \
    X(CLOUD_CONN_FAIL, "Kết nối thất bại", "Connection failed") \
    X(CLOUD_CONNECT_NET, "Đang kết nối…", "Connecting…") \
    X(CLOUD_CONNECTING_NET, "Đang kết nối…", "Connecting…") \
    X(CLOUD_COVER_FAIL, "Tải ảnh bìa thất bại", "Cover load failed") \
    X(CLOUD_DATA_FORMAT_ERR, "Sai định dạng dữ liệu", "Bad data format") \
    X(CLOUD_DECODE_FAIL, "Giải mã thất bại", "Decode failed") \
    X(CLOUD_DELETE_FAIL, "Xóa thất bại", "Delete failed") \
    X(CLOUD_DOWNLOAD_FAIL, "Tải xuống thất bại", "Download failed") \
    X(CLOUD_DOWNLOADING, "Đang tải xuống", "Downloading") \
    X(CLOUD_EMPTY_ALL, "Không có gì để truyền", "Nothing to transfer") \
    X(CLOUD_EMPTY_BOOK, "Không có sách", "No books") \
    X(CLOUD_EMPTY_FONT, "Không có phông", "No fonts") \
    X(CLOUD_EMPTY_WALLPAPER, "Không có hình nền", "No Walls") \
    X(CLOUD_FETCHING_LIST, "Đang tải danh sách…", "Fetching list…") \
    X(CLOUD_FILE_INVALID, "Tệp không hợp lệ", "Invalid file") \
    X(CLOUD_IN_QUEUE, "Đã có trong hàng đợi", "Already queued") \
    X(CLOUD_JSON_CREATE_FAIL, "Tạo JSON thất bại", "JSON create failed") \
    X(CLOUD_JSON_PARSE_FAIL, "Phân tích JSON thất bại", "JSON parse failed") \
    X(CLOUD_JSON_SERIALIZE_FAIL, "Tuần tự hóa JSON thất bại", "JSON serialize failed") \
    X(CLOUD_LIST_TOO_LARGE, "Danh sách quá lớn", "List too large") \
    X(CLOUD_LOAD_COVER, "Đang tải ảnh bìa…", "Loading covers…") \
    X(CLOUD_LOAD_FAIL, "Tải thất bại", "Load failed") \
    X(CLOUD_LOADING, "Đang tải…", "Loading…") \
    X(CLOUD_MISSING_TASK_ID, "Thiếu taskId", "Missing taskId") \
    X(CLOUD_NEED_WIFI_CFG, "Hãy cài đặt Wi-Fi trước", "Set up Wi-Fi first") \
    X(CLOUD_NET_NOT_READY, "Mạng chưa sẵn sàng", "Network not ready") \
    X(CLOUD_NO_COVER, "Không có bìa", "No cover") \
    X(CLOUD_NO_DOWNLOAD_URL, "Thiếu URL tải xuống", "Missing download URL") \
    X(CLOUD_NO_LOCAL_FILE, "Không có tệp trên máy", "No local file") \
    X(CLOUD_NO_NETWORK, "Không có mạng", "No network") \
    X(CLOUD_NO_SD, "Không có thẻ SD", "No SD card") \
    X(CLOUD_PATH_INVALID, "Đường dẫn không hợp lệ", "Invalid path") \
    X(CLOUD_PENDING, "Đang chờ nhận", "Incoming") \
    X(CLOUD_PLEASE_WAIT, "Vui lòng chờ", "Please wait") \
    X(CLOUD_PREVIEW_FAIL, "Không xem trước được", "Cannot preview") \
    X(CLOUD_PREVIEW_START_FAIL, "Không bắt đầu xem trước được", "Failed to start preview") \
    X(CLOUD_PREVIEW_URL_LONG, "URL xem trước quá dài", "Preview URL too long") \
    X(CLOUD_READ_TIMEOUT, "Đọc quá thời gian hoặc bị gián đoạn", "Read timeout or interrupted") \
    X(CLOUD_REFRESH, "Làm mới", "Refresh") \
    X(CLOUD_REFRESHED, "Đã làm mới", "Refreshed") \
    X(CLOUD_REFRESHING, "Đang làm mới…", "Refreshing…") \
    X(CLOUD_REQUEST_FAIL, "Yêu cầu thất bại", "Request failed") \
    X(CLOUD_FETCH_FAIL, "Không lấy được nội dung gửi đến. Kiểm tra mạng rồi thử lại.", "Failed to fetch incoming content. Check your network and try again.") \
    X(CLOUD_SAVE, "Lưu", "Save") \
    X(CLOUD_FILE_EXISTS, "Đã có tệp cùng tên", "A file with this name already exists") \
    X(CLOUD_CONFLICT_FMT, "Tệp đã tồn tại\n%s\nChọn cách lưu", "File already exists\n%s\nChoose how to save") \
    X(CLOUD_KEEP_BOTH, "Giữ cả hai", "Keep both") \
    X(CLOUD_SKIP_FILE, "Bỏ qua tệp", "Skip file") \
    X(CLOUD_REPLACE_FILE, "Thay thế tệp", "Replace file") \
    X(CLOUD_SAVE_FAIL, "Lưu thất bại", "Save failed") \
    X(CLOUD_SAVE_LOCAL, "Lưu vào thiết bị", "Save to device") \
    X(CLOUD_SAVED_0_1, "Đã lưu 0 / 1", "Saved 0 / 1") \
    X(CLOUD_SAVED_FMT, "Đã lưu %d / %d", "Saved %d / %d") \
    X(CLOUD_SAVED_SYNC_FAIL, "Đã lưu (đồng bộ thất bại)", "Saved (sync failed)") \
    X(CLOUD_SAVING, "Đang lưu", "Saving") \
    X(CLOUD_SAVING_NAME_FMT, "Đang lưu\n%s", "Saving\n%s") \
    X(CLOUD_SELECT_FIRST, "Hãy chọn mục trước", "Select items first") \
    X(CLOUD_SELECTED_FMT, "Đã chọn %d", "Selected %d") \
    X(CLOUD_STORAGE_UNAVAIL, "Không dùng được bộ nhớ", "Storage unavailable") \
    X(CLOUD_SYNC_FAIL, "Đồng bộ thất bại", "Sync failed") \
    X(CLOUD_TAB_ALL, "Tất cả", "All") \
    X(CLOUD_TAB_APP, "Đẩy từ APP", "APP Push") \
    X(CLOUD_TAB_AP, "Điểm phát", "Hotspot") \
    X(CLOUD_TAB_USB, "USB", "USB") \
    X(CLOUD_AP_STEP1, "Bước 1", "Step 1") \
    X(CLOUD_AP_JOIN, "Kết nối điểm phát, hoặc quét mã", "Join the hotspot, or scan the code") \
    X(CLOUD_AP_SSID_FMT, "Wi-Fi: %s", "Wi-Fi: %s") \
    X(CLOUD_AP_STEP2, "Bước 2", "Step 2") \
    X(CLOUD_AP_OPEN, "Mở địa chỉ này trong trình duyệt:", "Open this address in a browser:") \
    X(CLOUD_AP_HINT, "Hoặc quét mã bên dưới", "Or scan the code below") \
    X(CLOUD_AP_OPENING, "Đang bật điểm phát...", "Starting hotspot...") \
    X(CLOUD_AP_FAIL, "Không bật được điểm phát", "Hotspot failed to start") \
    X(CLOUD_AP_FAIL_INIT, "Khởi tạo Wi-Fi thất bại", "Wi-Fi init failed") \
    X(CLOUD_AP_FAIL_START, "Không bật được điểm phát", "Hotspot failed to start") \
    X(CLOUD_AP_FAIL_HTTP, "Không khởi động được trang web", "Web page failed to start") \
    X(CLOUD_WIFI_FAIL, "Kết nối Wi-Fi thất bại", "Wi-Fi connection failed") \
    X(CLOUD_CELLULAR_NEED_WIFI, "Đang dùng 4G. Chuyển sang Wi-Fi rồi thử lại.", "Using 4G. Switch to Wi-Fi and try again.") \
    X(CLOUD_USB_WAY, "Cách thức", "Method") \
    X(CLOUD_TAB_BOOK, "Sách", "Books") \
    X(CLOUD_TAB_FONT, "Phông", "Fonts") \
    X(CLOUD_TAB_WALLPAPER, "Hình nền", "Walls") \
    X(CLOUD_TASK_FAIL, "Tác vụ thất bại", "Task failed") \
    X(CLOUD_TYPE_BOOK, "Sách", "Book") \
    X(CLOUD_TYPE_FONT, "Phông", "Font") \
    X(CLOUD_TYPE_WALLPAPER, "Hình nền", "Walls") \
    X(CLOUD_UNKNOWN_TYPE, "Loại không rõ", "Unknown type") \
    X(CLOUD_WAIT_DOWNLOAD, "Chờ tải xuống xong", "Wait for download to finish") \
    X(COMMON_DELETE, "Xóa", "Delete") \
    X(COMMON_EMPTY, "Trống", "Empty") \
    X(COMMON_OFF, "Tắt", "Off") \
    X(COMMON_ON, "Bật", "On") \
    X(STANDBY_CONN_FAIL, "Kết nối thất bại", "Connection failed") \
    X(STANDBY_DATE_FMT, "%02d/%02d", "%02d/%02d") \
    X(STANDBY_DATE_PLACEHOLDER, "--/--", "--/--") \
    X(STANDBY_DECODE_FAIL, "Giải mã thất bại", "Decode failed") \
    X(STANDBY_EMPTY_TODO, "Không có việc cần làm", "No todos") \
    X(STANDBY_NO_DATA, "Không có dữ liệu", "No data") \
    X(STANDBY_NO_NETWORK, "Không có mạng", "No network") \
    X(STANDBY_NO_TODO_CACHE, "Chưa có công việc lưu sẵn", "No task cache") \
    X(STANDBY_NO_WEATHER, "Không có thời tiết", "No weather") \
    X(STANDBY_PARSE_FAIL, "Phân tích thất bại", "Parse failed") \
    X(STANDBY_REQUEST_FAIL, "Yêu cầu thất bại", "Request failed") \
    X(STANDBY_WEATHER_FAIL, "Lỗi thời tiết", "Weather failed") \
    X(STANDBY_WP_NOT_FOUND, "Không tìm thấy hình nền chờ", "Standby wallpaper not found") \
    X(WALLPAPER_ADJUST, "Điều chỉnh", "Adjust") \
    X(WALLPAPER_CHIP_SHUTDOWN, "Tắt máy", "Power-off") \
    X(WALLPAPER_CHIP_STANDBY, "Chờ", "Standby") \
    X(WALLPAPER_DECODE_FAIL, "Giải mã thất bại", "Decode failed") \
    X(WALLPAPER_DELETE_BTN, "Xóa hình nền", "Delete wallpaper") \
    X(WALLPAPER_DELETE_FAIL, "Xóa thất bại", "Delete failed") \
    X(WALLPAPER_DELETE_START_FAIL, "Không bắt đầu được việc xóa", "Failed to start delete") \
    X(WALLPAPER_DELETING, "Đang xóa…", "Deleting…") \
    X(WALLPAPER_EMPTY_FMT, "Chưa có hình nền\nĐặt vào %s", "No wallpapers\nPlace in %s") \
    X(WALLPAPER_ENABLE_FAIL, "Bật thất bại", "Enable failed") \
    X(WALLPAPER_FILE_INVALID, "Tệp không hợp lệ", "Invalid file") \
    X(WALLPAPER_FILE_MISSING, "Không tìm thấy tệp", "File not found") \
    X(WALLPAPER_LOADING, "Đang tải…", "Loading…") \
    X(WALLPAPER_ENTER_LOADING, "Đang tải hình nền...", "Loading wallpaper...") \
    X(WALLPAPER_MIRROR_H, "Lật ngang", "Mirror") \
    X(WALLPAPER_NAME_INVALID, "Tên tệp không hợp lệ", "Invalid filename") \
    X(WALLPAPER_NO_SD, "Không có thẻ SD", "No SD card") \
    X(WALLPAPER_OVERWRITE, "Ghi đè", "Overwrite") \
    X(WALLPAPER_PATH_INVALID, "Đường dẫn không hợp lệ", "Invalid path") \
    X(WALLPAPER_PREVIEW_OOM, "Hết bộ nhớ, xem trước thất bại", "Out of memory, preview failed") \
    X(WALLPAPER_ROTATE_LEFT, "Trái", "Left") \
    X(WALLPAPER_ROTATE_RIGHT, "Phải", "Right") \
    X(WALLPAPER_SAVE, "Lưu", "Save") \
    X(WALLPAPER_SAVE_AS, "Lưu thành", "Save as") \
    X(WALLPAPER_SAVE_FAIL, "Lưu thất bại", "Save failed") \
    X(WALLPAPER_SAVING, "Đang lưu…", "Saving…") \
    X(WALLPAPER_SELECTED_FMT, "Đã chọn %d", "Selected %d") \
    X(WALLPAPER_SET_SHUTDOWN, "Đặt làm màn tắt máy", "Set as power-off") \
    X(WALLPAPER_SET_STANDBY, "Đặt làm màn chờ", "Set as standby") \
    X(WALLPAPER_SET_READING, "Đặt làm nền đọc sách", "Set as reading background") \
    X(WALLPAPER_READING_ON, "Đã chọn nền đọc sách", "Reading background selected") \
    X(WALLPAPER_DISPLAY_MODE, "Chế độ hiển thị", "Display mode") \
    X(WALLPAPER_DECODE_FAIL_CODE, "Giải mã thất bại (%u)", "Decode failed (%u)") \
    X(WALLPAPER_MODE_FULLSCREEN, "Hình nền toàn màn", "Fullscreen wallpaper") \
    X(WALLPAPER_MODE_OVERLAY, "Hình nền trong suốt", "Transparent wallpaper") \
    X(WALLPAPER_CLEAR_SELECTION, "Bỏ chọn", "Clear selection") \
    X(WALLPAPER_SHUTDOWN_ON, "Đã chọn màn tắt máy", "Power-off selected") \
    X(WALLPAPER_STANDBY_ON, "Đã chọn màn chờ", "Standby selected")


namespace Lang {

enum class LangStringId : size_t {
#define LANG_ENUM(name, vi, en) name,
    LANG_STRING_LIST(LANG_ENUM)
#undef LANG_ENUM
    kCount,
};

static constexpr size_t kStringCount = static_cast<size_t>(LangStringId::kCount);

static const char* const kVi[kStringCount] = {
#define LANG_VI(name, vi, en) vi,
    LANG_STRING_LIST(LANG_VI)
#undef LANG_VI
};

static const char* const kEn[kStringCount] = {
#define LANG_EN(name, vi, en) en,
    LANG_STRING_LIST(LANG_EN)
#undef LANG_EN
};

const char* CODE = LANG_DEFAULT_CODE;

const char* BookHomeLabel(bool vietnamese) {
    const size_t index = static_cast<size_t>(LangStringId::BOOK_TOC_HOME);
    return vietnamese ? kVi[index] : kEn[index];
}

namespace Strings {
#define LANG_DECL(name, vi, en) const char* name = "";
LANG_STRING_LIST(LANG_DECL)
#undef LANG_DECL
} // namespace Strings

// 仅支持 vi-VN / en-US；旧固件保存的 zh-CN 等其它值回退 en-US
static const char* NormalizeCode(const char* code) {
    if (code != nullptr && std::strcmp(code, "vi-VN") == 0) {
        return "vi-VN";
    }
    return "en-US";
}

static const char* const* LiteralsFor(const char* code) {
    if (code != nullptr && std::strcmp(code, "vi-VN") == 0) {
        return kVi;
    }
    return kEn;
}

static void ApplyLiterals(const char* const* literals) {
#define LANG_APPLY(name, vi, en)                                                                               \
    Strings::name = literals[static_cast<size_t>(LangStringId::name)];
    LANG_STRING_LIST(LANG_APPLY)
#undef LANG_APPLY
}

static void PersistTask(void* arg) {
    const char* code = static_cast<const char*>(arg);
    {
        Settings settings("ui", true);
        settings.SetString("language", code);
    }
    ESP_LOGI(TAG, "language persisted: %s", code);
    vTaskDelete(nullptr);
}

const char* Current() {
    return CODE;
}

bool SetLanguage(const char* code, bool persist) {
    const char* normalized = NormalizeCode(code);
    const bool changed = (CODE == nullptr) || (std::strcmp(CODE, normalized) != 0);
    ApplyLiterals(LiteralsFor(normalized));
    CODE = normalized;
    if (persist) {
        // LVGL worker 栈在 PSRAM：写 NVS 放到内部 RAM 栈任务
        if (xTaskCreate(PersistTask, "lang_nvs", 4096, const_cast<char*>(normalized), 5, nullptr) !=
            pdPASS) {
            ESP_LOGE(TAG, "xTaskCreate(lang_nvs) failed");
        }
    }
    return changed;
}

void InitFromNvs() {
    Settings settings("ui", false);
    const std::string saved = settings.GetString("language", "");
    if (saved.empty()) {
        SetLanguage(LANG_DEFAULT_CODE, false);
    } else {
        SetLanguage(saved.c_str(), false);
    }
}

namespace {
struct LangStaticInit {
    LangStaticInit() {
        Lang::SetLanguage(LANG_DEFAULT_CODE, false);
    }
};
LangStaticInit g_lang_static_init;
} // namespace

} // namespace Lang
