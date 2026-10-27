#include "VirtualKeyboard.h"
#include "../../../NexoBar.h"
#include <algorithm>

Rectangle VirtualKeyboard::keyArea(const std::size_t index) const {
    constexpr std::size_t columns=10; const float gap=4.f;
    const float w=(m_area.width-gap*9.f)/10.f,h=(m_area.height-gap*3.f)/4.f;
    return {m_area.x+static_cast<float>(index%columns)*(w+gap),m_area.y+static_cast<float>(index/columns)*(h+gap),w,h};
}
void VirtualKeyboard::update(float){visible=UIInput::touchMode()&&static_cast<bool>(m_target());if(!visible||!UIInput::pointerReleased())return;const auto p=UIInput::pointerPosition();for(std::size_t i=0;i<m_keys.size();++i)if(CheckCollisionPointRec(p,keyArea(i))){m_target()->insertVirtual(static_cast<char>(std::tolower(static_cast<unsigned char>(m_keys[i]))));return;}const auto space=keyArea(30);const Rectangle wide{space.x,space.y,space.width*7+24,space.height};const auto del=keyArea(37);const Rectangle erase{del.x,del.y,del.width*3+8,del.height};if(CheckCollisionPointRec(p,wide))m_target()->insertVirtual(' ');else if(CheckCollisionPointRec(p,erase))m_target()->backspaceVirtual();}
void VirtualKeyboard::draw(){if(!visible||!font)return;for(std::size_t i=0;i<m_keys.size()&&i<30;++i){const auto a=keyArea(i);DrawRectangleRounded(a,.1f,4,Color{42,55,72,255});char label[2]{m_keys[i],0};const auto s=MeasureTextEx(*font,label,18,1);DrawTextEx(*font,label,{a.x+(a.width-s.x)/2,a.y+(a.height-s.y)/2},18,1,RAYWHITE);}const auto base=keyArea(30);const Rectangle space{base.x,base.y,base.width*7+24,base.height};const auto d=keyArea(37);const Rectangle erase{d.x,d.y,d.width*3+8,d.height};DrawRectangleRounded(space,.1f,4,Color{42,55,72,255});DrawRectangleRounded(erase,.1f,4,Color{75,48,52,255});NexoBarImp app;const auto spaceText=app->text("keyboard.space");const auto deleteText=app->text("keyboard.delete");auto measure=MeasureTextEx(*font,spaceText.c_str(),17,1);DrawTextEx(*font,spaceText.c_str(),{space.x+(space.width-measure.x)/2,space.y+10},17,1,RAYWHITE);measure=MeasureTextEx(*font,deleteText.c_str(),17,1);DrawTextEx(*font,deleteText.c_str(),{erase.x+(erase.width-measure.x)/2,erase.y+10},17,1,RAYWHITE);}
