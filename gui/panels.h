#ifndef RT_PANELS_H
#define RT_PANELS_H

#include <QDockWidget>
#include <vector>

#include "scene.h"

class QListWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;

// Object outliner: lists scene objects and offers add / delete / duplicate.
class OutlinerPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit OutlinerPanel(QWidget* parent = nullptr);

    void refresh(const std::vector<SceneObject>& objects, int selected);

signals:
    void selectionChanged(int index);
    void addRequested(ShapeType type);
    void deleteRequested();
    void duplicateRequested();

private:
    QListWidget* list_;
    QComboBox*   shapeCombo_;
};

// Properties of the selected object: transform + material. Emits the fully
// updated object on any edit.
class PropertiesPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void setObject(const SceneObject& obj);
    void clear();

signals:
    void objectChanged(const SceneObject& obj);

private:
    void pull();             // read widgets into current_, then emit
    void updateColorSwatch();

    QDoubleSpinBox* pos_[3];
    QDoubleSpinBox* rot_[3];
    QDoubleSpinBox* scl_[3];
    QComboBox*      matType_;
    QPushButton*    colorBtn_;
    QDoubleSpinBox* fuzz_;
    QDoubleSpinBox* ior_;

    SceneObject current_;
    bool        has_ = false;
    bool        loading_ = false;   // suppress emits while populating
};

// Movable light: position (x/y/z), intensity, color, plus sky/background dimming.
class LightPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit LightPanel(QWidget* parent = nullptr);

    void set(const Light& light, float background);

signals:
    void lightChanged(const Light& light, float background);

private:
    void pull();
    void updateColorSwatch();

    QDoubleSpinBox* pos_[3];
    QDoubleSpinBox* intensity_;
    QPushButton*    colorBtn_;
    QDoubleSpinBox* background_;

    Light current_;
    bool  loading_ = false;
};

// Render quality / output controls.
class RenderPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit RenderPanel(QWidget* parent = nullptr);

signals:
    void resolutionChanged(int width, int height);
    void maxDepthChanged(int depth);
    void maxSamplesChanged(int samples);

private:
    QComboBox* resCombo_;
    QSpinBox*  depth_;
    QSpinBox*  samples_;
};

#endif


