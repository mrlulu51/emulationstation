#pragma once
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/OptionListComponent.h"

class GuiProfileSelector : public GuiComponent {
    public:
        GuiProfileSelector(Window* window);

        bool input(InputConfig* config, Input input) override;

        void update(int deltaTime) override;
        void render(const Transform4x4f& parentTrans) override;
    
    private:
        MenuComponent mMenu;
        void buildMenu();

        std::vector<std::shared_ptr<OptionListComponent<std::string>>> mSelectors;
};