#pragma once

#include <QDialog>
#include <QString>

namespace Ui {
class DataSourceUploadDialog;
}

// 数据源上传配置对话框。
//
// 对应界面文件：datasourceuploaddialog.ui
// 用户在这里选择文件路径、总线类型和数据源名称。
class DataSourceUploadDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造上传配置对话框。
    // @param parent Qt 父窗口。
    explicit DataSourceUploadDialog(QWidget *parent = nullptr);

    // 销毁 Designer UI。
    ~DataSourceUploadDialog() override;

    // 获取选择的文件路径。
    // @return 文件绝对路径。
    QString filePath() const;

    // 获取选择的总线类型。
    // @return FC、Aurora 或 以太网。
    QString fileType() const;

    // 获取用户输入的数据源名称。
    // @return 数据源名称。
    QString dataSourceName() const;

private slots:
    // 打开文件选择对话框。
    void browseFile();

    // 校验输入并在通过后关闭对话框。
    void validateAndAccept();

private:
    Ui::DataSourceUploadDialog *ui;
};
