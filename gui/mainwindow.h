#ifndef RT_MAINWINDOW_H
#define RT_MAINWINDOW_H

#include <QMainWindow>

#include "scene.h"

class RenderView;
class RenderWorker;
class OutlinerPanel;
class PropertiesPanel;
class LightPanel;
class RenderPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent*) override;

private slots:
    void onAdd(ShapeType type);
    void onDelete();
    void onDuplicate();
    void onSelect(int index);
    void onPicked(int index);
    void onObjectEdited(const SceneObject& obj);
    void onLightChanged(const Light& light, float background);
    void onGizmoTranslated(float dx, float dy, float dz);

private:
    void pushScene();      // re-bake on the render thread
    void syncSelection();  // refresh outliner + properties for selected_
    void updateGizmo();    // place the viewport gizmo on the selected object

    RenderView*      view_     = nullptr;
    RenderWorker*    worker_   = nullptr;
    OutlinerPanel*   outliner_ = nullptr;
    PropertiesPanel* props_    = nullptr;
    LightPanel*      lightPanel_ = nullptr;
    RenderPanel*     renderPanel_ = nullptr;

    SceneDesc desc_;
    int       selected_ = -1;
};

#endif
