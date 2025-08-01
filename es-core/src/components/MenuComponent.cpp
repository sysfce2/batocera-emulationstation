#include "components/MenuComponent.h"
#include "components/ButtonComponent.h"
#include "components/MultiLineMenuEntry.h"
#include "TextToSpeech.h"

#define BUTTON_GRID_VERT_PADDING  (Renderer::getScreenHeight()*0.0296296)
#define BUTTON_GRID_HORIZ_PADDING (Renderer::getScreenWidth()*0.0052083333)

#define TITLE_HEIGHT (mTitle->getFont()->getLetterHeight() + (mSubtitle ? TITLE_WITHSUB_VERT_PADDING : TITLE_VERT_PADDING) + (mSubtitle ? mSubtitle->getSize().y() + SUBTITLE_VERT_PADDING : 0))

MenuComponent::MenuComponent(Window* window, 
	const std::string& title, const std::shared_ptr<Font>& titleFont,
	const std::string& subTitle, bool tabbedUI) 
	: GuiComponent(window),
	mBackground(window), mGrid(window, Vector2i(1, 4)), mTabIndex(0)
{
	if (tabbedUI)
	{
		// Tabs
		mTabs = std::make_shared<ComponentTab>(mWindow);
		mTabs->setCursorChangedCallback([&](const CursorState& /*state*/)
			{
				int idx = mTabs->getCursorIndex();
				if (mTabIndex != idx)
				{
					mTabIndex = idx;
					OnTabChanged(idx);					
				}
			});
	}

	mMaxHeight = 0;

	auto theme = ThemeData::getMenuTheme();

	addChild(&mBackground);
	addChild(&mGrid);

	mGrid.setZIndex(10);

	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);
	mBackground.setZIndex(2);

	// set up title
	mTitle = std::make_shared<TextComponent>(mWindow);
	mTitle->setHorizontalAlignment(ALIGN_CENTER);
	mTitle->setColor(theme->Title.color); // 0x555555FF

	if (theme->Title.selectorColor != 0x555555FF)
	{
		mTitle->setBackgroundColor(theme->Title.selectorColor);
		mTitle->setRenderBackground(true);
	}

	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(2, 2));
	mHeaderGrid->setColWidthPerc(0, 1);
	mHeaderGrid->setColWidthPerc(1, 0);

	mHeaderGrid->setEntry(mTitle, Vector2i(0, 0), false, true);

	setTitle(title, theme->Title.font); //  titleFont
	setSubTitle(subTitle);

	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);
	//mGrid.setEntry(mTitle, Vector2i(0, 0), false);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool {
		if (config->isMappedLike("down", input)) {
			mGrid.setCursorTo(mList);
			mList->setCursorIndex(0);
			return true;
		}
		if (config->isMappedLike("up", input)) {
			mList->setCursorIndex(mList->size() - 1);
			if (mButtons.size()) {
				mGrid.moveCursor(Vector2i(0, 1));
			}
			else {
				mGrid.setCursorTo(mList);
			}
			return true;
		}
		return false;
	});

	// set up list which will never change (externally, anyway)
	mList = std::make_shared<ComponentList>(mWindow);

	if (mTabs != nullptr)
		mGrid.setEntry(mTabs, Vector2i(0, 1), false, true);

	mGrid.setEntry(mList, Vector2i(0, 2), true);

	updateGrid();
	updateSize();

	mGrid.resetCursor();
}

bool MenuComponent::input(InputConfig* config, Input input)
{	
	if (mTabs != nullptr && mTabs->size() && mGrid.getSelectedComponent() != mButtonGrid && (config->isMappedLike("left", input) || config->isMappedLike("right", input)))
	{
		bool ret = mTabs->input(config, input);

		if (input.type != TYPE_HAT || input.value != 0) 
			return ret;

		// continue processing if the HAT value was reset to 0
	}

	if (GuiComponent::input(config, input))
		return true;

	return false;
}

void MenuComponent::addTab(const std::string label, const std::string value, bool setCursorHere)
{
	if (mTabs != nullptr)
		mTabs->addTab(label, value, setCursorHere);
}

void MenuComponent::addMenuIcon(Window* window, ComponentListRow& row, const std::string& iconName)
{
	if (iconName.empty())
		return;
		
	auto theme = ThemeData::getMenuTheme();

	std::string iconPath = theme->getMenuIcon(iconName);
	if (!iconPath.empty())
	{
		// icon
		auto icon = std::make_shared<ImageComponent>(window, true);
		icon->setImage(iconPath);
		icon->setColorShift(theme->Text.color);
		icon->setResize(0, theme->Text.font->getLetterHeight() * 1.25f);
		row.addElement(icon, false);

		// spacer between icon and text
		auto spacer = std::make_shared<GuiComponent>(window);
		spacer->setSize(10, 0);
		row.addElement(spacer, false);

		return;
	}

	std::string label;
	if (iconName == "audio")
		label = _U("\uf028");
	else if (iconName == "keyboard")
		label = _U("\uf11c");
	else if (iconName == "joystick")
		label = _U("\uf11b");
	else if (iconName == "mouse")
		label = _U("\uf124");
	else if (iconName == "unknown")
		label = _U("\uf1de");
	
	if (!label.empty())
	{
		auto text = std::make_shared<TextComponent>(window, label, theme->Text.font, theme->Text.color, ALIGN_CENTER);
		row.addElement(text, false);

		// spacer between icon and text
		auto spacer = std::make_shared<GuiComponent>(window);
		spacer->setSize(10, 0);
		row.addElement(spacer, false);
	}
}

void MenuComponent::addWithLabel(const std::string& label, const std::shared_ptr<GuiComponent>& comp, const std::function<void()>& func, const std::string& iconName, bool setCursorHere)
{
	auto theme = ThemeData::getMenuTheme();

	ComponentListRow row;

	addMenuIcon(mWindow, row, iconName);

	auto text = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(label), theme->Text.font, theme->Text.color);
	row.addElement(text, true);

	if (EsLocale::isRTL())
		text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

	if (comp != nullptr)
		row.addElement(comp, false);

	if (func != nullptr)
		row.makeAcceptInputHandler(func);

	addRow(row, setCursorHere);
}

void MenuComponent::addWithDescription(const std::string& label, const std::string& description, const std::shared_ptr<GuiComponent>& comp, const std::function<void()>& func, const std::string& iconName, bool setCursorHere, bool multiLine, const std::string& userData, bool doUpdateSize)
{
	auto theme = ThemeData::getMenuTheme();

	ComponentListRow row;

	addMenuIcon(mWindow, row, iconName);

	if (!description.empty())
	{
		if (!multiLine)
			mList->setUpdateType(ComponentListFlags::UpdateType::UPDATE_ALWAYS);			

		row.addElement(std::make_shared<MultiLineMenuEntry>(mWindow, Utils::String::toUpper(label), description, multiLine), true);
	}
	else
		row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(label), theme->Text.font, theme->Text.color), true);

	if (comp != nullptr)
		row.addElement(comp, false);

	if (func != nullptr)
		row.makeAcceptInputHandler(func);


	addRow(row, setCursorHere, doUpdateSize, userData);
}

void MenuComponent::addEntry(const std::string& name, bool add_arrow, const std::function<void()>& func, const std::string& iconName, bool setCursorHere, bool onButtonRelease, const std::string& userData, bool doUpdateSize)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// populate the list
	ComponentListRow row;

	addMenuIcon(mWindow, row, iconName);

	auto text = std::make_shared<TextComponent>(mWindow, name, font, color);
	row.addElement(text, true);

	if (EsLocale::isRTL())
		text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

	if (add_arrow)
	{
		auto arrow = makeArrow(mWindow);

		if (EsLocale::isRTL())
			arrow->setFlipX(true);

		row.addElement(arrow, false);
	}

	if (func != nullptr)
		row.makeAcceptInputHandler(func, onButtonRelease);

	addRow(row, setCursorHere, doUpdateSize, userData);
}

void MenuComponent::setTitle(const std::string& title, const std::shared_ptr<Font>& font)
{
	mTitle->setText(Utils::String::toUpper(title));
	
	if (font != nullptr)
		mTitle->setFont(font);
}

void MenuComponent::setTitleImage(std::shared_ptr<ImageComponent> titleImage, bool replaceTitle)
{
	if (titleImage == nullptr)
	{
		if (mTitleImage != nullptr)
		{
			mHeaderGrid->removeEntry(mTitleImage);
			mTitleImage = nullptr;
		}

		mHeaderGrid->setColWidthPerc(0, 1);
		mHeaderGrid->setColWidthPerc(1, 0);

		return;
	}

	mTitleImage = titleImage;	
	mTitleImage->setPadding(TITLE_HEIGHT * 0.15f);
	
	if (replaceTitle)
	{		
		mTitleImage->setMaxSize(mSize.x() * 0.85f, TITLE_HEIGHT);

		mHeaderGrid->setColWidthPerc(0, 0);
		mHeaderGrid->setColWidthPerc(1, 1);
		mHeaderGrid->setEntry(mTitleImage, Vector2i(0, 0), false, false, Vector2i(2, 2));
		
		if (mTitle)
			mTitle->setVisible(false);

		if (mSubtitle)
			mSubtitle->setVisible(false);
	}
	else
	{
		float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));
		float iw = TITLE_HEIGHT / width;

		mTitleImage->setMaxSize(1.3f * iw * mSize.x(), TITLE_HEIGHT);

		mHeaderGrid->setColWidthPerc(0, 1 - iw);
		mHeaderGrid->setColWidthPerc(1, iw);
		mHeaderGrid->setEntry(mTitleImage, Vector2i(1, 0), false, false, Vector2i(1, 2));
	}

	updateSize();	
}

void MenuComponent::setSubTitle(const std::string& text)
{
	if (text.empty())
	{
		if (mSubtitle != nullptr)
		{
			mHeaderGrid->removeEntry(mSubtitle);
			mSubtitle = nullptr;			
		}

		mHeaderGrid->setRowHeightPerc(0, 1);
		mHeaderGrid->setRowHeightPerc(1, 0);

		return;
	}
	
	if (mSubtitle == nullptr)
	{
		auto theme = ThemeData::getMenuTheme();

		mSubtitle = std::make_shared<TextComponent>(mWindow, 
			Utils::String::toUpper(Utils::FileSystem::getFileName(text)),
			theme->TextSmall.font, theme->TextSmall.color, ALIGN_CENTER);

		mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 1), false, true);
	}
	
	mSubtitle->setText(text);
	mSubtitle->setVerticalAlignment(Alignment::ALIGN_TOP);
	mSubtitle->setSize(Renderer::getScreenWidth() * 0.88f, 0);
	mSubtitle->setLineSpacing(1.1);
	
	const float titleHeight = mTitle->getFont()->getLetterHeight() + (mSubtitle ? TITLE_WITHSUB_VERT_PADDING : TITLE_VERT_PADDING);
	const float subtitleHeight = mSubtitle->getSize().y() + SUBTITLE_VERT_PADDING;

	mHeaderGrid->setRowHeightPerc(0, titleHeight / TITLE_HEIGHT);	

	if (mTitleImage != nullptr)
		setTitleImage(mTitleImage);

	updateSize();
}

float MenuComponent::getTitleHeight() const
{
	return TITLE_HEIGHT;
}

float MenuComponent::getHeaderGridHeight() const
{
	return mHeaderGrid->getRowHeight(0);
}

float MenuComponent::getButtonGridHeight() const
{
	auto menuTheme = ThemeData::getMenuTheme();
	return (mButtonGrid ? mButtonGrid->getSize().y() : menuTheme->Text.font->getHeight() + BUTTON_GRID_VERT_PADDING);	
}

void MenuComponent::updateSize()
{
	// GPI
	if (Renderer::ScreenSettings::fullScreenMenus())
	{
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		return;
	}

	const float maxHeight = mMaxHeight <= 0 ? Renderer::getScreenHeight() * 0.75f : mMaxHeight;

	float height = TITLE_HEIGHT + mList->getTotalRowHeight() + getButtonGridHeight() + 2;
	if (mTabs != nullptr && mTabs->size())
		height += mTabs->getSize().y();

	if(height > maxHeight)
	{
		height = TITLE_HEIGHT + getButtonGridHeight();
		int i = 0;
		while(i < mList->size())
		{
			float rowHeight = mList->getRowHeight(i);
			if(height + rowHeight < maxHeight)
				height += rowHeight;
			else
				break;
			i++;
		}
	}

	float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));
	setSize(width, height);
	
	if (mTitleImage != nullptr && mTitle != nullptr && mTitle->isVisible())
	{
		float pad = Renderer::getScreenWidth() * 0.012;
		mTitle->setPadding(Vector4f(pad, 0.0f, pad, 0.0f));
		mTitle->setHorizontalAlignment(ALIGN_LEFT);

		if (mSubtitle != nullptr)
		{
			mSubtitle->setPadding(Vector4f(pad, 0.0f, pad, 0.0f));
			mSubtitle->setHorizontalAlignment(ALIGN_LEFT);
		}
	}
}

void MenuComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	// update grid row/col sizes
	mGrid.setRowHeight(0, TITLE_HEIGHT, false);
	mGrid.setRowHeight(3, getButtonGridHeight(), false);

	if (mTabs == nullptr || mTabs->size() == 0)
		mGrid.setRowHeight(1, 0.00001f);
	else
	{
		const float titleHeight = mTitle->getFont()->getLetterHeight();
		mGrid.setRowHeight(1, titleHeight * 2);
	}

	mGrid.setSize(mSize);

	// Fix size if the Title Image replaces the title
	if (mTitleImage != nullptr && (mTitle == nullptr || !mTitle->isVisible()))
	{		
		mTitleImage->setOrigin(0.5f, 0.5f);
		mTitleImage->setPosition(getPosition().x() + mSize.x() / 2.0f, getPosition().y() + TITLE_HEIGHT / 2.0f);
		mTitleImage->setMaxSize(mSize.x() * 0.85f, TITLE_HEIGHT);
	}
}

void MenuComponent::clearButtons()
{
	mButtons.clear();
	updateGrid();
	updateSize();
}

void MenuComponent::addButton(const std::string& name, const std::string& helpText, const std::function<void()>& callback)
{
	mButtons.push_back(std::make_shared<ButtonComponent>(mWindow, Utils::String::toUpper(name), helpText, callback));
	updateGrid();
	updateSize();
}

void MenuComponent::updateGrid()
{
	if(mButtonGrid)
		mGrid.removeEntry(mButtonGrid);

	mButtonGrid.reset();

	if (mButtons.size())
	{
		mButtonGrid = makeButtonGrid(mWindow, mButtons);
		mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false);
	}
}

std::vector<HelpPrompt> MenuComponent::getHelpPrompts()
{
	return mGrid.getHelpPrompts();
}

void MenuComponent::onShow()
{
  GuiComponent::onShow();
  //TextToSpeech::getInstance()->say(mTitle->getText());
  getList()->saySelectedLine();
}

std::shared_ptr<ComponentGrid> makeButtonGrid(Window* window, const std::vector< std::shared_ptr<ButtonComponent> >& buttons)
{
	std::shared_ptr<ComponentGrid> buttonGrid = std::make_shared<ComponentGrid>(window, Vector2i((int)buttons.size(), 2));

	float buttonGridWidth = (float)BUTTON_GRID_HORIZ_PADDING * buttons.size(); // initialize to padding
	for(int i = 0; i < (int)buttons.size(); i++)
	{
		buttonGrid->setEntry(buttons.at(i), Vector2i(i, 0), true, false);
		buttonGridWidth += buttons.at(i)->getSize().x();
	}
	for(unsigned int i = 0; i < buttons.size(); i++)
	{
		buttonGrid->setColWidthPerc(i, (buttons.at(i)->getSize().x() + BUTTON_GRID_HORIZ_PADDING) / buttonGridWidth);
	}

	buttonGrid->setSize(buttonGridWidth, buttons.at(0)->getSize().y() + BUTTON_GRID_VERT_PADDING + 2);
	buttonGrid->setRowHeightPerc(1, 2 / buttonGrid->getSize().y()); // spacer row to deal with dropshadow to make buttons look centered

	return buttonGrid;
}

std::shared_ptr<ImageComponent> makeArrow(Window* window)
{
	auto menuTheme = ThemeData::getMenuTheme();

	auto bracket = std::make_shared<ImageComponent>(window);
	bracket->setImage(ThemeData::getMenuTheme()->Icons.arrow); // ":/arrow.svg");
	bracket->setColorShift(menuTheme->Text.color);
	bracket->setResize(0, round(menuTheme->Text.font->getLetterHeight()));

	return bracket;
}
