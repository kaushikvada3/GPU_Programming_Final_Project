#include "mainwindow.h"

#include "renderview.h"
#include "renderworker.h"
#include "panels.h"

#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("RT Studio — real-time GPU ray tracer");

    desc_ = defaultScene();

    view_ = new RenderView(this);
    setCentralWidget(view_);

    outliner_ = new OutlinerPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, outliner_);
    outliner_->refresh(desc_.objects, selected_);

    props_ = new PropertiesPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, props_);
    props_->clear();

    lightPanel_ = new LightPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, lightPanel_);
    lightPanel_->set(desc_.light, desc_.background);

    renderPanel_ = new RenderPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, renderPanel_);

    worker_ = new RenderWorker(800, 450, desc_, this);

    connect(worker_, &RenderWorker::frameReady, view_, &RenderView::onFrame);
    connect(worker_, &RenderWorker::sceneFramed, view_, &RenderView::onSceneFramed);
    connect(worker_, &RenderWorker::objectPicked, this, &MainWindow::onPicked);
    connect(props_, &PropertiesPanel::objectChanged, this, &MainWindow::onObjectEdited);
    connect(lightPanel_, &LightPanel::lightChanged, this, &MainWindow::onLightChanged);
    connect(renderPanel_, &RenderPanel::resolutionChanged, this,
            [this](int w, int h) { worker_->setResolution(w, h); });
    connect(renderPanel_, &RenderPanel::maxDepthChanged, this,
            [this](int d) { worker_->setMaxDepth(d); });
    connect(renderPanel_, &RenderPanel::maxSamplesChanged, this,
            [this](int s) { worker_->setMaxSamples(s); });

    connect(view_, &RenderView::cameraChanged, this,
            [this](const OrbitCamera& c) { worker_->setCamera(c); });
    connect(view_, &RenderView::interacting, this,
            [this](bool on) { worker_->setInteracting(on); });
    connect(view_, &RenderView::pickRequested, this,
            [this](float nx, float ny) { worker_->requestPick(nx, ny); });
    connect(view_, &RenderView::gizmoTranslated, this, &MainWindow::onGizmoTranslated);

    connect(outliner_, &OutlinerPanel::addRequested, this, &MainWindow::onAdd);
    connect(outliner_, &OutlinerPanel::deleteRequested, this, &MainWindow::onDelete);
    connect(outliner_, &OutlinerPanel::duplicateRequested, this, &MainWindow::onDuplicate);
    connect(outliner_, &OutlinerPanel::selectionChanged, this, &MainWindow::onSelect);

    tabifyDockWidget(props_, lightPanel_);
    tabifyDockWidget(lightPanel_, renderPanel_);
    props_->raise();

    worker_->start();
    resize(1180, 720);
}

MainWindow::~MainWindow() = default;

void MainWindow::pushScene() {
    worker_->setScene(desc_);
}

void MainWindow::syncSelection() {
    outliner_->refresh(desc_.objects, selected_);
    if (selected_ >= 0 && selected_ < int(desc_.objects.size()))
        props_->setObject(desc_.objects[selected_]);
    else
        props_->clear();
    updateGizmo();
}

void MainWindow::updateGizmo() {
    if (selected_ >= 0 && selected_ < int(desc_.objects.size())) {
        const auto& o = desc_.objects[selected_];
        view_->setGizmo(o.position[0], o.position[1], o.position[2], true);
    } else {
        view_->setGizmo(0, 0, 0, false);
    }
}

void MainWindow::onAdd(ShapeType type) {
    desc_.objects.push_back(makeObject(type));
    selected_ = int(desc_.objects.size()) - 1;
    syncSelection();
    pushScene();
}

void MainWindow::onDelete() {
    if (selected_ < 0 || selected_ >= int(desc_.objects.size()))
        return;
    desc_.objects.erase(desc_.objects.begin() + selected_);
    if (selected_ >= int(desc_.objects.size()))
        selected_ = int(desc_.objects.size()) - 1;
    syncSelection();
    pushScene();
}

void MainWindow::onDuplicate() {
    if (selected_ < 0 || selected_ >= int(desc_.objects.size()))
        return;
    SceneObject copy = desc_.objects[selected_];
    copy.position[0] += 1.0f;
    copy.name += " copy";
    desc_.objects.push_back(copy);
    selected_ = int(desc_.objects.size()) - 1;
    syncSelection();
    pushScene();
}

void MainWindow::onSelect(int index) {
    selected_ = index;
    if (selected_ >= 0 && selected_ < int(desc_.objects.size()))
        props_->setObject(desc_.objects[selected_]);
    else
        props_->clear();
    updateGizmo();
}

void MainWindow::onPicked(int index) {
    if (index < 0)
        return;
    selected_ = index;
    syncSelection();
}

void MainWindow::onObjectEdited(const SceneObject& obj) {
    if (selected_ < 0 || selected_ >= int(desc_.objects.size()))
        return;
    desc_.objects[selected_] = obj;
    updateGizmo();
    pushScene();
}

void MainWindow::onGizmoTranslated(float dx, float dy, float dz) {
    if (selected_ < 0 || selected_ >= int(desc_.objects.size()))
        return;
    auto& o = desc_.objects[selected_];
    o.position[0] += dx;
    o.position[1] += dy;
    o.position[2] += dz;
    props_->setObject(o);   // keep the spinboxes in sync
    updateGizmo();
    pushScene();
}

void MainWindow::onLightChanged(const Light& light, float background) {
    desc_.light = light;
    desc_.background = background;
    pushScene();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (worker_)
        worker_->stop();
    e->accept();
}
