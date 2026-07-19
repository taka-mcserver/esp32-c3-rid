// Wrapper for embedded HTML
// The actual HTML is in web/index.html, embedded via EMBED_TXTFILES
extern const char web_index_html_start[] asm("_binary_web_index_html_start");
extern const char web_index_html_end[]   asm("_binary_web_index_html_end");
#define INDEX_HTML web_index_html_start
