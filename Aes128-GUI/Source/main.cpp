#define _AMD64_
#include "Ange.h"
#include "Aes.h"
#include <sys/stat.h> //For stat (checking if path is dir)
#include <fstream>
using namespace Ange;

std::list<std::string> filesTemp;
std::list<std::string> toProcess;
std::atomic<bool> done(false);

bool CheckIfFile(std::string path)
{
	struct stat s;
	if (stat(path.c_str(), &s) == 0)
	{
		if (s.st_mode & S_IFREG)
		{
			return true;
		}
	}
	return false;
}

bool CheckIfCryptedFormat(std::string path)
{
	if (path.size() < 5) return false;
	if (path.substr(path.size() - 5) == ".a128") return true;
	return false;
}

char* LoadFile(std::string path, size_t & size)
{
	std::fstream file;
	file.open(path.c_str(), std::ios::in | std::ios::binary);
	if (file.good() == true)
	{

		file.seekg(0, std::ios_base::end);
		size = file.tellg();
		file.seekg(0, std::ios_base::beg);

		char* bufor = new char[size];
		file.read(bufor, size);

		file.close();
		return bufor;
	}
	std::cerr << "Cannot find file: " << path << std::endl;
	return nullptr;
}

void CreateFileFromBuffer(std::string path, char* data, size_t size)
{
	std::fstream file(path.c_str(), std::ios::out | std::ios::binary);
	file.write(data, size);
	file.close();
}


class FileRow : public CustomWidget {
public:
	FileRow(Window* window, Widget2DProps props, TextTheme theme, std::string path, VScroller* scr) :
		CustomWidget(window, props)
	{
		//Add components
		AddComponent(0, new SimpleButton<Background>(
			window,
			props,
			SimpleButtonTheme{
				{{205,45,45,0}, {0,0,0,0}},
				{{205,45,45,255}, {0,0,0,255}},
				{{225,45,45,255}, {0,0,0,255}},
				{0,0},
				{0, {255,255,255,255}, nullptr},
				{0.0f, 0.0f, 0.0f, 0.0f}
			}
		));
		AddComponent(1, new Text(window, props, theme, utf8_decode(path)));

		//Fix size
		auto text = (Text*)GetComponent(1);
		auto btn = (SimpleButton<Background>*)GetComponent(0);
		btn->Resize({ props.Dimensions.tWidth, text->GetDimension().tHeight });
		m_Widget2DProps.Dimensions.tHeight = text->GetDimension().tHeight;

		//Setup callback
		btn->SetCallback([&scr, this, path](Event*ev) {
			if (ev->GetEventType() == EventType::MouseClick) {
				MouseClickEvent* mce = (MouseClickEvent*)ev;
				if (mce->GetButton() == 0 && mce->GetAction() == 0) {
					scr->Remove(this);

					for (std::list<std::string>::iterator p = toProcess.begin(); p != toProcess.end();)
					{
						if ((*p) == path) {
							p = toProcess.erase(p);
						}
						else {
							++p;
						}
					}
					//DisableWidget();
					delete this;
					return true;
				}
			}
			return false;
		});

		//Add event handler
		m_Bindings.push_back(m_ParentWindow->BindEvent(EventType::WindowResize, I_BIND(FileRow, OnWindowResize)));
	}

	FileRow::~FileRow()
	{
		DisableWidget();
	}

	//Add event handler
	void EnableWidget() override
	{

		m_Bindings.push_back(m_ParentWindow->BindEvent(EventType::WindowResize, I_BIND(FileRow, OnWindowResize)));
		CustomWidget::EnableWidget();
	}

	void DisableWidget() override
	{
		if (m_ParentWindow == nullptr) return;
		for (auto it : m_Bindings)
		{
			m_ParentWindow->UnbindEvent(it);
		}
		m_Bindings.clear();
		CustomWidget::DisableWidget();
	}

	bool OnWindowResize(Event* ev)
	{
		//Fix size
		auto text = (Text*)GetComponent(1);
		auto btn = (SimpleButton<Background>*)GetComponent(0);
		btn->Resize({ m_Widget2DProps.Dimensions.tWidth, text->GetDimension().tHeight });
		m_Widget2DProps.Dimensions.tHeight = text->GetDimension().tHeight;
		return false;
	}

	void SetFlags(int flags) override
	{
		for (auto it : m_Components) {
			//Change only anchor flags
			int newflags = (it.second->GetFlags() & AnchorMask) | (flags & AnchorBits);
			it.second->SetFlags(newflags);
		}
	}

};


void ProcessingThread(Window* window, ProgressBar* pb, SimpleButton<Background>* workBtn, VScroller* scr, SimpleInput* sInp)
{
	uint8_t initv[16];
	char key[16];
	memset(initv, 0, 16);
	memset(key, 0, 16);
	memcpy(key, sInp->GetText().c_str(), sInp->GetTextRef().size());

	float f1 = 0.0f;
	pb->SetToObserve(&f1);
	
	int i = 0;
	for(auto p : toProcess) {
	
		Aes aes;
		size_t size;
		size_t outputSize;

		char* data = LoadFile(p, size);

		if (!CheckIfCryptedFormat(p)){ //Crypt
			uint8_t* crypted = aes.CryptData(data, size, key, (uint8_t*)initv, outputSize);
			CreateFileFromBuffer(p+".a128", (char*)crypted, outputSize);
			delete[] crypted;

		} else  { //Decrypt
			char* decrypted = aes.DecryptData((uint8_t*)data, size, key, (uint8_t*)initv, outputSize);
			CreateFileFromBuffer(p.substr(0, p.size()-5), (char*)decrypted, outputSize);
			delete[] decrypted;
		}
		delete[] data;
		++i;
		f1 = (float)i / toProcess.size();
	}
	pb->SetToObserve(nullptr);

	//Change progressbar to button
	pb->DisableWidget();
	workBtn->EnableWidget();
	//Raise WindowResize event for button to reposition
	window->RaiseEvent(new WindowResizeEvent(window->GetDimension().tWidth, window->GetDimension().tHeight));
	//Clean list
	//scr->Clear();
	done = true;
}


int main()
{
	//-----------------------------------------------------------------------------------
	//Init
	//-----------------------------------------------------------------------------------

	//Create main window
	Window window(
		nullptr,
		"Aes128 Tool",
		{ {300,50}, {500,400}, WindowFlags::ChildAutoOperate | WindowFlags::AutoInvokeRender | WindowFlags::FifoDrawable }
	);
	window.Init();
	window.SetMinMaxDimensions(500, 400, -1, -1);
	window.SetClearColor(Color{ 255,255,255,255 });

	//Load main font
	Font font("segoeui.ttf");

	//Create sub-theme and attatch font
	Theme theme = DefTheme;
	theme.SimpleButtonBG.TextTh.Tint = { 255,255,255,255 };
	theme.SimpleButtonBG.Base->Tint = { 0,0,0,255 };
	theme.AssignFontToAll(&font);

	//-----------------------------------------------------------------------------------
	//Create scene
	//-----------------------------------------------------------------------------------

	Background bg(
		&window,
		{ {0,0}, {0,0}, Anchor::Left | Anchor::Bottom | ResizePolicy::AutoFill },
		{ {32,41,66,255}, {23,29,51,255}, {1,1} }
	);
	Background filesHeaderBg(
		&window,
		{ {0,(int)window.GetDimension().tHeight - 38}, {window.GetDimension().tWidth,38}, Anchor::Left | Anchor::Bottom },
		{ {39,47,75,255}, {25,32,54,255}, {1,1} }
	);
	Background optionsHeaderBg(
		&window,
		{ {0,140}, {window.GetDimension().tWidth,38}, Anchor::Left | Anchor::Bottom },
		{ {39,47,75,255}, {25,32,54,255}, {1,1} }
	);

	Background workBg(
		&window,
		{ {0,0}, {window.GetDimension().tWidth,80}, Anchor::Left | Anchor::Bottom },
		{ {39,47,75,255}, {25,32,54,255}, {1,1} }
	);

	Text filesTxt(
		&window,
		{ {20,(int)window.GetDimension().tHeight - 31}, {200,(size_t)font.GetLineHeight(14)}, Anchor::Left | Anchor::Bottom },
		{ 14, {255,255,255,255}, &font },
		L"Files"
	);
	Text dropTxt(
		&window,
		{ {(int)window.GetDimension().tWidth / 2,(int)window.GetDimension().tHeight - 131}, {300,(size_t)font.GetLineHeight(14)}, Anchor::HorizontalCenter | Anchor::VerticalCenter },
		{ 14, {108,108,108,255}, &font },
		L"Drag & Drop files here"
	);
	Text optionsTxt(
		&window,
		{ {20, 147}, {200,(size_t)font.GetLineHeight(14)}, Anchor::Left | Anchor::Bottom },
		{ 14, {255,255,255,255}, &font },
		L"Password"
	);
	SimpleInput passInp(
		&window,
		{ {250,110}, {350,30}, Anchor::HorizontalCenter | Anchor::VerticalCenter },
		{
			{{80,183,235,255}, {0,0,0,0}},
			{{77,150,150,255}, {0,0,0,0}},
			{{40,143,205,255}, {0,0,0,0}},
			{0,0},
			{12, {20,20,20,180}, &font},
			{12, {255,255,255,255}, &font},
			{20,20,20,255},
			{255,255,255,255},
			{20,20,20,255},
			{4,4},
			1
		},
		L"Please enter crypt/decrypt password."
	);
	SimpleButton<Rectangle2D> workBtn(
		&window,
		{ {250,40}, {350,30}, Anchor::HorizontalCenter | Anchor::VerticalCenter },
		SimpleButtonTheme{
			{{205,45,45,255}, {0,0,0,255}},
			{{253,53,53,255}, {0,0,0,255}},
			{{225,45,45,255}, {0,0,0,255}},
			{0,0},
			{15, {255,255,255,255}, &font},
			{0.0f, 0.0f, 0.0f, 0.0f}
		},
		L"Process files"
	);
	ProgressBar pb(
		&window,
		{ {250,40}, {350,30}, Anchor::HorizontalCenter | Anchor::VerticalCenter | ProgressBarFlags::PrecentageInfo | ProgressBarFlags::AutoUpdate | ProgressBarFlags::InvokeCallbackOnDone },
		theme,
		L"Processing... ",
		1.0f
	);
	pb.DisableWidget();
	AreaWidget area(
		&window,
		{ {20, 190}, {410, 162}, Anchor::Left | Anchor::Bottom }
	);
	VScroller scr(
		&window,
		{ {460, 190}, {10, 162}, Anchor::Left | Anchor::Bottom | ScrollerFlags::AutoDisable },
		{
			{{0,0,0,0},{0,0,0,0}},
			{{193,193,193,255}, {0,0,0,0}},
			{{168,168,168,255}, {0,0,0,0}},
			{{120,120,120,255}, {0,0,0,0}},
			{0,0},
			{0,0},
			10
		},
		&area
	);
	scr.SetInsertOffsets({0, 10});

	//Set scaling data
	filesHeaderBg.SetResizeProportions(0, 100, 100, 0);
	optionsHeaderBg.SetResizeProportions(0, 0, 100, 0);
	workBg.SetResizeProportions(0, 0, 100, 0);
	filesTxt.SetResizeProportions(0, 100, 100, 0);
	dropTxt.SetResizeProportions(50, 50, 0, 0);
	optionsTxt.SetResizeProportions(0, 0, 100, 0);
	passInp.SetResizeProportions(50, 0, 0, 0);
	workBtn.SetResizeProportions(50, 0, 0, 0);
	pb.SetResizeProportions(50, 0, 0, 0);
	area.SetResizeProportions(0,0,100,100);
	scr.SetResizeProportions(100, 0, 0, 100);

	//-----------------------------------------------------------------------------------
	//Connect widgets
	//-----------------------------------------------------------------------------------

	std::thread* thread = nullptr;

	//Limit to 16 chars
	passInp.SetCallback([&passInp](Event* ev) {
		size_t s = passInp.GetTextRef().size();
		if (s > 16){
			passInp.SetText(passInp.GetText().substr(0,16));
		}
		return true;
	});

	//Make button work
	workBtn.SetCallback([&window, &workBtn, &pb, &thread, &scr, &passInp](Event* ev) {
		if (thread != nullptr) return true;
		if(ev->GetEventType() == EventType::MouseClick){
			MouseClickEvent* mce = (MouseClickEvent*)ev;
			if (mce->GetButton() == 0 && mce->GetAction() == 1){
				if (toProcess.size() != 0) {
					workBtn.DisableWidget();
					pb.EnableWidget();
					//Raise WindowResize event for pb to reposition
					window.RaiseEvent(new WindowResizeEvent(window.GetDimension().tWidth, window.GetDimension().tHeight));
					thread = new std::thread(ProcessingThread, &window, &pb, &workBtn, &scr, &passInp);
				}
			}
			return true;
		}
		return false;
	});

	/*pb.SetCallback([&workBtn, &pb](Event* ev){
		pb.DisableWidget();
		workBtn.EnableWidget();
		return true;
	});*/

	//Some GLFW code to detect drag n drop event (later will be as a part of ANGE lib)
	//Cannot capture other objects here because GLFW callbacks are C only. :((
	//Because of that, i will be polling data in main loop.
	glfwSetDropCallback(window.GetGLFWwindow(), [](GLFWwindow* window, int count, const char** paths){
		for (int i = 0; i < count; i++) {
			filesTemp.push_back(std::string(paths[i]));
		}
	});

	//-----------------------------------------------------------------------------------
	//Program Loop
	//-----------------------------------------------------------------------------------


	while (window.Operate())
	{
		//Add items
		if (filesTemp.size() != 0)
		{
			//Add Items (Removing is done via callback)
			for (auto fn : filesTemp) {
				//Check if already in list
				bool check = false;
				for (auto f : toProcess) {
					if (f == fn) {
						check = true;
						break;
					}
				}
				if (check == true) continue;
				//Check if path is file
				if (CheckIfFile(fn)) {
					//And finally add
					auto row = new FileRow(&window, { {0,0}, {window.GetDimension().tWidth - 90, 25}, TextFlags::Multiline | TextFlags::AutoHeight }, { 12, {255,255,255,255}, &font }, fn, &scr);
					row->SetResizeProportions(0,0,100,0);
					scr.PushBack(row);
					toProcess.push_back(fn);
				}
			}
			filesTemp.clear();

			//Update backgorund (tip) text
			if (scr.GetSize() != 0)
			{
				dropTxt.SetVisibility(false);
			} else {
				dropTxt.SetVisibility(true);
			}
		}

		//Clear Items
		if (done){
			scr.Clear();
			toProcess.clear();
			done = false;
			thread = nullptr;
		}

		window.ClearScene();
	}
	scr.Clear();

	return 0;
}
