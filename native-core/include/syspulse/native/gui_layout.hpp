// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <optional>
#include <cstddef>
namespace syspulse::native {
enum class GuiPage { Overview, Performance, Processes, Storage, Startup, Services, SystemTools, Network, Recovery, Reports };
struct GuiNav { GuiPage page; const char* shortLabel; const char* label; };
inline constexpr std::array<GuiNav,10> guiPages{{
 {GuiPage::Overview,"OV","System overview"},{GuiPage::Performance,"PF","Performance"},{GuiPage::Processes,"PM","Task manager"},{GuiPage::Storage,"ST","Storage & cleanup"},
 {GuiPage::Startup,"SU","Startup apps"},{GuiPage::Services,"SV","Packages & services"},{GuiPage::SystemTools,"TO","System tools"},
 {GuiPage::Network,"NW","Network & power"},{GuiPage::Recovery,"RE","Backup & recovery"},{GuiPage::Reports,"RP","Desktop & reports"}
}};
struct GuiRect {int x{},y{},w{},h{}; constexpr bool contains(int a,int b)const noexcept{return a>=x&&b>=y&&a-x<w&&b-y<h;}
 constexpr bool operator==(const GuiRect& r)const noexcept{return x==r.x&&y==r.y&&w==r.w&&h==r.h;}};
constexpr int metricColumns(int content) noexcept{return content>=840?4:2;}
constexpr int metricsBottom(int content) noexcept{return 153+4/metricColumns(content)*88;}
constexpr int performanceRows(int content,int height) noexcept{const int rows=(height-135-metricsBottom(content)-64)/27;return rows<1?1:rows>12?12:rows;}
struct GuiLayout {
 int sidebar{},content{};
 explicit constexpr GuiLayout(int width):sidebar(width>=1100?228:width>=700?52:0),content(width-sidebar){}
 constexpr bool expanded()const noexcept{return sidebar>=200;}
 constexpr int navY(std::size_t i)const noexcept{return 83+static_cast<int>(i)*43+(i>=4?20:0)+(i>=8?20:0);}
 constexpr std::optional<GuiPage> navHit(int x,int y)const noexcept{if(!sidebar)return {};for(std::size_t i=0;i<guiPages.size();++i)if(GuiRect{8,navY(i),sidebar-16,36}.contains(x,y))return guiPages[i].page;return {};}
};
}
