// 对应界面：datasourceuploaddialog.ui。
// 本文件负责选择文件、回填默认名称并校验上传参数。
#include "datasourceuploaddialog.h"

#include "ui_datasourceuploaddialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

namespace {

// DataSourceUploadDialog 的“完成”“取消”按钮统一样式。
// 参数与配色与项目主界面中的 Dialog 按钮保持一致，直接设置到按钮本身，
// 避免只设置 QDialogButtonBox 时内部 QPushButton 无法正确继承样式。
QString dialogActionButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "min-width: 76px;"
        "min-height: 30px;"
        "background-color: #ffffff;"
        "color: #36546a;"
        "border: 1px solid #a9bfd0;"
        "border-radius: 1px;"
        "padding: 0 12px;"
        "}"
        "QPushButton:hover {"
        "background-color: #edf6f8;"
        "border-color: #79b7c3;"
        "}"
        "QPushButton:pressed {"
        "background-color: #e0eef1;"
        "border-color: #6fb5c0;"
        "}"
        "QPushButton:disabled {"
        "background-color: #f8fafb;"
        "color: #a6b5bf;"
        "border-color: #d6e0e7;"
        "}");
}

// 对 QDialogButtonBox 中的标准按钮设置文案和样式。
// @param buttonBox 包含标准按钮的按钮盒。
// @param standardButton 需要设置的标准按钮类型。
// @param text 按钮显示文字。
void applyDialogActionButtonStyle(QDialogButtonBox *buttonBox,
                                  QDialogButtonBox::StandardButton standardButton,
                                  const QString &text)
{
    if (buttonBox == nullptr) {
        return;
    }

    QPushButton *button = buttonBox->button(standardButton);
    if (button == nullptr) {
        return;
    }

    button->setText(text);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(dialogActionButtonStyle());
}

} // namespace

DataSourceUploadDialog::DataSourceUploadDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DataSourceUploadDialog)
{
    ui->setupUi(this);
    applyDialogActionButtonStyle(
        ui->buttonBox,
        QDialogButtonBox::Ok,
        QStringLiteral("完成"));
    applyDialogActionButtonStyle(
        ui->buttonBox,
        QDialogButtonBox::Cancel,
        QStringLiteral("取消"));

    connect(ui->browseButton,
            &QPushButton::clicked,
            this,
            &DataSourceUploadDialog::browseFile);
    connect(ui->buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &DataSourceUploadDialog::validateAndAccept);
    connect(ui->buttonBox,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject);
}

DataSourceUploadDialog::~DataSourceUploadDialog()
{
    delete ui;
}

QString DataSourceUploadDialog::filePath() const
{
    return ui->filePathEdit->text().trimmed();
}

QString DataSourceUploadDialog::fileType() const
{
    return ui->fileTypeCombo->currentText();
}

QString DataSourceUploadDialog::dataSourceName() const
{
    return ui->dataSourceNameEdit->text().trimmed();
}

void DataSourceUploadDialog::browseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择数据源文件"),
        QString(),
        QStringLiteral("BIN 文件 (*.bin);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    // 用户选择文件后立即回填完整路径。
    ui->filePathEdit->setText(path);
    // 数据源名称未填写时，默认使用文件名去掉扩展名后的部分。
    // 需要修改默认命名规则时，改这里的 QFileInfo(...).completeBaseName()。
    if (ui->dataSourceNameEdit->text().trimmed().isEmpty()) {
        ui->dataSourceNameEdit->setText(QFileInfo(path).completeBaseName());
    }
}

void DataSourceUploadDialog::validateAndAccept()
{
    const QFileInfo info(filePath());
    if (filePath().isEmpty() || !info.exists() || !info.isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("输入不完整"),
            QStringLiteral("请选择存在的文件路径。"));
        return;
    }

    if (dataSourceName().isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("输入不完整"),
            QStringLiteral("请输入数据源名称。"));
        return;
    }

    accept();
}
