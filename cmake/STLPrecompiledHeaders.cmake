include_guard(GLOBAL)

set(EMERAUDE_BASE_STL_PCH_HEADERS
	"<filesystem>"
	"<chrono>"
	"<iostream>"
	"<memory>"
	"<format>"
	"<algorithm>"
	"<streambuf>"
	"<string>"
	"<functional>"
	"<future>"
	"<variant>"
	"<sstream>"
	"<array>"
	"<map>"
	"<vector>"
	"<ranges>"
	"<atomic>"
	CACHE INTERNAL "emeraude-base shared STL hot-set for the precompiled headers."
)
