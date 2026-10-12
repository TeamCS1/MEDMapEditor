#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "../src/core/bookmarks.h"

using namespace med;

static int failures = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        if (!(c)) {                                               \
            std::printf("FAIL line %d: %s\n", __LINE__, #c);      \
            ++failures;                                           \
        }                                                         \
    } while (0)

int main(int argc, char** argv) {
    // MEd 0.32's own file, as shipped
    const std::string medFile =
        "<bookmark id=0>GTA III - INDUSTSE - Portland Hospital</bookmark><location id=0>1145.68:-576.017:13.528:15.6316:204.8:358.4:0</location>\r\n"
        "<bookmark id=1>GTA VC - OCEANDRV - Ocean Hotel</bookmark><location id=1>229.921:-1262.81:19.484:25.0519:102:359.2:0</location>\r\n"
        "<bookmark id=2>San Andreas - LAe2 - Grove Street</bookmark><location id=2>2494.15:-1669.84:51.0703:55.3825:5.8:358:0</location>\r\n"
        "\r\n";
    std::vector<Bookmark> b = parseBookmarks(medFile);
    CHECK(b.size() == 3);
    CHECK(b[0].name == "GTA III - INDUSTSE - Portland Hospital" && b[0].x == 1145.68 && b[0].y == -576.017 && b[0].z == 13.528 && b[0].h == 15.6316 &&
          b[0].yaw == 204.8 && b[0].pitch == 358.4 && b[0].flag == 0);
    CHECK(b[2].name == "San Andreas - LAe2 - Grove Street" && b[2].pitch == 358);
    CHECK(writeBookmarks(b) == medFile);  // byte for byte what MEd wrote

    // the real file, when given
    if (argc > 1) {
        std::ifstream f(argv[1], std::ios::binary);
        std::stringstream s;
        s << f.rdbuf();
        CHECK(writeBookmarks(parseBookmarks(s.str())) == s.str());
    }

    // edits keep the format: renumbered in order
    std::vector<Bookmark> e = b;
    std::swap(e[0], e[2]);
    e.erase(e.begin() + 1);
    Bookmark n;
    n.name = "My spot";
    n.x = 1.5;
    n.y = -2.25;
    n.z = 1000000;
    n.h = 0;
    n.yaw = 359.999;
    n.pitch = 0.001;
    e.push_back(n);
    const std::string w = writeBookmarks(e);
    CHECK(w.find("<bookmark id=0>San Andreas") == 0);
    CHECK(w.find("<bookmark id=2>My spot</bookmark><location id=2>1.5:-2.25:1e+06:0:359.999:0.001:0</location>") != std::string::npos);
    std::vector<Bookmark> back = parseBookmarks(w);
    CHECK(back.size() == 3 && back[2].name == "My spot" && back[2].z == 1000000 && back[1].name == e[1].name);

    // damaged input: skipped, never a crash
    CHECK(parseBookmarks("").empty());
    CHECK(parseBookmarks("<bookmark id=0>no location</bookmark>\r\n").empty());
    CHECK(parseBookmarks("<bookmark id=0>a</bookmark><location id=0>1:2:3</location>").empty());               // too few numbers
    CHECK(parseBookmarks("<bookmark id=0>a</bookmark><location id=0>1:2:x:4:5:6:0</location>").empty());      // not a number
    CHECK(parseBookmarks("<bookmark id=0>a</bookmark><location id=0>1:2:3:4:5:6</location>").size() == 1);    // the flag may be missing
    CHECK(parseBookmarks("<bookmark id=5>a</bookmark><location id=5>1:2:3:4:5:6:0</location>\n<bookmark id=1>b</bookmark><location id=1>1:2:3:4:5:6:0</location>")[0].name == "b");  // id order
    CHECK(parseBookmarks("<bookmark id=x>a</bookmark><location id=x>1:2:3:4:5:6:0</location>").empty());
    CHECK(parseBookmarks("<bookmark id=0>a</bookmark><location id=0>1:2:3:4:5:6:0").empty());                  // cut off
    CHECK(parseBookmarks(std::string(100000, '<')).empty());

    CHECK(validBookmarkName("Portland docks") && !validBookmarkName("") && !validBookmarkName("   ") && !validBookmarkName("a<b") && !validBookmarkName("a>b") &&
          !validBookmarkName("line\nbreak"));

    if (failures == 0) std::printf("all bookmark tests passed\n");
    return failures ? 1 : 0;
}
