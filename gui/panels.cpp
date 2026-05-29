#include "panels.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QColorDialog>

OutlinerPanel::OutlinerPanel(QWidget* parent) : QDockWidget("Scene", parent) {
    auto* root = new QWidget(this);
    auto* col = new QVBoxLayout(root);

    list_ = new QListWidget(root);
    col->addWidget(list_, 1);

    auto* addRow = new QHBoxLayout();
    shapeCombo_ = new QComboBox(root);
    shapeCombo_->addItem("Box",      int(ShapeType::Box));
    shapeCombo_->addItem("Sphere",   int(ShapeType::Sphere));
    shapeCombo_->addItem("Prism",    int(ShapeType::Prism));
    shapeCombo_->addItem("Pyramid",  int(ShapeType::Pyramid));
    shapeCombo_->addItem("Cylinder", int(ShapeType::Cylinder));
    auto* addBtn = new QPushButton("Add", root);
    addRow->addWidget(shapeCombo_, 1);
    addRow->addWidget(addBtn);
    col->addLayout(addRow);

    auto* btnRow = new QHBoxLayout();
    auto* dupBtn = new QPushButton("Duplicate", root);
    auto* delBtn = new QPushButton("Delete", root);
    btnRow->addWidget(dupBtn);
    btnRow->addWidget(delBtn);
    col->addLayout(btnRow);

    setWidget(root);

    connect(list_, &QListWidget::currentRowChanged, this,
            &OutlinerPanel::selectionChanged);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        emit addRequested(ShapeType(shapeCombo_->currentData().toInt()));
    });
    connect(delBtn, &QPushButton::clicked, this, &OutlinerPanel::deleteRequested);
    connect(dupBtn, &QPushButton::clicked, this, &OutlinerPanel::duplicateRequested);
}

void OutlinerPanel::refresh(const std::vector<SceneObject>& objects, int selected) {
    QSignalBlocker block(list_);
    list_->clear();
    for (const auto& o : objects)
        list_->addItem(QString::fromStdString(o.name));
    if (selected >= 0 && selected < int(objects.size()))
        list_->setCurrentRow(selected);
}

// ---- PropertiesPanel ----

static QDoubleSpinBox* makeSpin(double lo, double hi, double step) {
    auto* s = new QDoubleSpinBox();
    s->setRange(lo, hi);
    s->setSingleStep(step);
    s->setDecimals(2);
    return s;
}

PropertiesPanel::PropertiesPanel(QWidget* parent) : QDockWidget("Properties", parent) {
    auto* root = new QWidget(this);
    auto* col = new QVBoxLayout(root);

    auto* xform = new QGroupBox("Transform", root);
    auto* box = new QVBoxLayout(xform);
    col->addWidget(xform);
    auto vecRow = [&](const char* label, QDoubleSpinBox* s[3],
                      double lo, double hi, double step) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(label));
        for (int k = 0; k < 3; k++) {
            s[k] = makeSpin(lo, hi, step);
            row->addWidget(s[k]);
            connect(s[k], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { pull(); });
        }
        box->addLayout(row);
    };
    vecRow("Pos",   pos_, -100.0, 100.0, 0.25);
    vecRow("Rot",   rot_, -360.0, 360.0, 5.0);
    vecRow("Scale", scl_,   0.05, 100.0, 0.25);

    auto* matBox = new QGroupBox("Material", root);
    {
        auto* form = new QFormLayout(matBox);
        col->addWidget(matBox);
        matType_ = new QComboBox();
        matType_->addItem("Diffuse",  int(MaterialType::Diffuse));
        matType_->addItem("Metal",    int(MaterialType::Metal));
        matType_->addItem("Glass",    int(MaterialType::Glass));
        matType_->addItem("Emissive", int(MaterialType::Emissive));
        form->addRow("Type", matType_);

        colorBtn_ = new QPushButton("Color");
        form->addRow("Color", colorBtn_);

        fuzz_ = makeSpin(0.0, 1.0, 0.05);
        form->addRow("Fuzz", fuzz_);
        ior_ = makeSpin(1.0, 3.0, 0.05);
        form->addRow("IOR", ior_);

        connect(matType_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { pull(); });
        connect(fuzz_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { pull(); });
        connect(ior_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { pull(); });
        connect(colorBtn_, &QPushButton::clicked, this, [this]() {
            QColor init = QColor::fromRgbF(current_.material.color[0],
                                           current_.material.color[1],
                                           current_.material.color[2]);
            QColor c = QColorDialog::getColor(init, this, "Albedo");
            if (c.isValid()) {
                current_.material.color[0] = float(c.redF());
                current_.material.color[1] = float(c.greenF());
                current_.material.color[2] = float(c.blueF());
                updateColorSwatch();
                if (!loading_ && has_) emit objectChanged(current_);
            }
        });
    }

    col->addStretch(1);
    setWidget(root);
    clear();
}

void PropertiesPanel::updateColorSwatch() {
    QColor c = QColor::fromRgbF(current_.material.color[0],
                               current_.material.color[1],
                               current_.material.color[2]);
    colorBtn_->setStyleSheet(QString("background-color: %1;").arg(c.name()));
}

void PropertiesPanel::setObject(const SceneObject& obj) {
    loading_ = true;
    has_ = true;
    current_ = obj;
    for (int k = 0; k < 3; k++) {
        pos_[k]->setValue(obj.position[k]);
        rot_[k]->setValue(obj.rotation[k]);
        scl_[k]->setValue(obj.scale[k]);
    }
    int mi = matType_->findData(int(obj.material.type));
    matType_->setCurrentIndex(mi < 0 ? 0 : mi);
    fuzz_->setValue(obj.material.fuzz);
    ior_->setValue(obj.material.ior);
    updateColorSwatch();
    setEnabled(true);
    loading_ = false;
}

void PropertiesPanel::clear() {
    has_ = false;
    setEnabled(false);
}

void PropertiesPanel::pull() {
    if (loading_ || !has_)
        return;
    for (int k = 0; k < 3; k++) {
        current_.position[k] = float(pos_[k]->value());
        current_.rotation[k] = float(rot_[k]->value());
        current_.scale[k]    = float(scl_[k]->value());
    }
    current_.material.type = MaterialType(matType_->currentData().toInt());
    current_.material.fuzz = float(fuzz_->value());
    current_.material.ior  = float(ior_->value());
    emit objectChanged(current_);
}

// ---- LightPanel ----

LightPanel::LightPanel(QWidget* parent) : QDockWidget("Light", parent) {
    auto* root = new QWidget(this);
    auto* form = new QFormLayout(root);

    auto* posRow = new QHBoxLayout();
    const char* axis[3] = {"X", "Y", "Z"};
    for (int k = 0; k < 3; k++) {
        pos_[k] = makeSpin(-100.0, 100.0, 0.5);
        posRow->addWidget(new QLabel(axis[k]));
        posRow->addWidget(pos_[k]);
        connect(pos_[k], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { pull(); });
    }
    form->addRow("Position", posRow);

    intensity_ = makeSpin(0.0, 100.0, 0.5);
    form->addRow("Intensity", intensity_);
    connect(intensity_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { pull(); });

    colorBtn_ = new QPushButton("Color");
    form->addRow("Color", colorBtn_);
    connect(colorBtn_, &QPushButton::clicked, this, [this]() {
        QColor init = QColor::fromRgbF(current_.color[0], current_.color[1], current_.color[2]);
        QColor c = QColorDialog::getColor(init, this, "Light color");
        if (c.isValid()) {
            current_.color[0] = float(c.redF());
            current_.color[1] = float(c.greenF());
            current_.color[2] = float(c.blueF());
            updateColorSwatch();
            if (!loading_) pull();
        }
    });

    background_ = makeSpin(0.0, 1.0, 0.05);
    form->addRow("Sky", background_);
    connect(background_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { pull(); });

    setWidget(root);
}

void LightPanel::updateColorSwatch() {
    QColor c = QColor::fromRgbF(current_.color[0], current_.color[1], current_.color[2]);
    colorBtn_->setStyleSheet(QString("background-color: %1;").arg(c.name()));
}

void LightPanel::set(const Light& light, float background) {
    loading_ = true;
    current_ = light;
    for (int k = 0; k < 3; k++)
        pos_[k]->setValue(light.position[k]);
    intensity_->setValue(light.intensity);
    background_->setValue(background);
    updateColorSwatch();
    loading_ = false;
}

void LightPanel::pull() {
    if (loading_)
        return;
    for (int k = 0; k < 3; k++)
        current_.position[k] = float(pos_[k]->value());
    current_.intensity = float(intensity_->value());
    emit lightChanged(current_, float(background_->value()));
}

// ---- RenderPanel ----

RenderPanel::RenderPanel(QWidget* parent) : QDockWidget("Render", parent) {
    auto* root = new QWidget(this);
    auto* form = new QFormLayout(root);

    resCombo_ = new QComboBox();
    resCombo_->addItem("600 x 338",  QSize(600, 338));
    resCombo_->addItem("800 x 450",  QSize(800, 450));
    resCombo_->addItem("1280 x 720", QSize(1280, 720));
    resCombo_->setCurrentIndex(1);
    form->addRow("Resolution", resCombo_);

    depth_ = new QSpinBox();
    depth_->setRange(1, 50);
    depth_->setValue(10);
    form->addRow("Max bounces", depth_);

    samples_ = new QSpinBox();
    samples_->setRange(8, 4096);
    samples_->setValue(256);
    samples_->setSingleStep(32);
    form->addRow("Sample cap", samples_);

    setWidget(root);

    connect(resCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                QSize s = resCombo_->currentData().toSize();
                emit resolutionChanged(s.width(), s.height());
            });
    connect(depth_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RenderPanel::maxDepthChanged);
    connect(samples_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RenderPanel::maxSamplesChanged);
}
