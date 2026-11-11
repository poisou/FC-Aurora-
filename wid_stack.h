#pragma once

#include <QMap>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class WidReview;
class wid_customdatasource;
class wid_datasource;

namespace Ui {
class wid_stack;
}

// 固定外壳 + 内容页面栈：
// 对应界面文件：wid_stack.ui
//   固定区域 = 顶部产品栏、五个功能切页、底部状态栏
//   内容页面 = 设备管理、数据源管理、数据回看（WidReview）
//
// “数据源管理”显示独立页面；“数据回看”以嵌入方式显示 WidReview。
class wid_stack : public QWidget
{
    Q_OBJECT

public:
    // 构造设备管理页面栈。
    // @param parent Qt 父控件，页面销毁时由父对象自动管理。
    explicit wid_stack(QWidget *parent = nullptr);

    // 销毁页面及 Qt Designer 创建的 UI 对象。
    ~wid_stack() override;

    // 从命令行或外部入口选择一个 BIN 文件。
    // @param filePath 待打开的 BIN 文件绝对或相对路径。
    // @return 文件被接受并已切换到数据回看页时返回 true。
    bool loadFile(const QString &filePath);

private slots:
    // 显示设备管理内容页，并激活“设备管理”切页。
    void showDeviceManagementPage();

    // 显示独立的数据源管理内容页，不进入 WidReview。
    void showDataSourcePage();

    // 在固定导航外壳下方显示 WidReview 页面。
    void showReviewPage();

    // 根据设备树选择刷新右侧设备信息。
    // @param current 当前选中的设备树节点。
    // @param previous 之前选中的设备树节点，本实现不使用。
    void handleDeviceSelection(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    // 切换当前设备打开/关闭状态，并启动或停止对应状态定时器。
    void handleOpenDevice();

    // 当前设备状态定时器超时回调。
    // @param deviceName 该定时器对应的唯一设备名称。
    void handleDeviceStatusTick(const QString &deviceName);

    // 更新右侧连接状态文本及其颜色。
    // @param connected true 显示绿色“已连接”，false 显示灰色“离线”。
    void setDeviceStatusText(bool connected);

    // 更新设备树中对应设备的圆点，并同步对应接口状态。
    // @param deviceName 设备树中的唯一设备名称。
    // @param connected true 显示绿色在线状态，false 显示灰色离线状态。
    void updateDeviceConnectionVisual(const QString &deviceName, bool connected);

    // 更新设备类型对应的接口连接状态。
    // @param deviceType 设备类型，可取 FC、Aurora、以太网。
    // @param connected true 显示绿色“在线”，false 显示灰色“离线”。
    void updateInterfaceConnectionVisual(const QString &deviceType, bool connected);

private:
    // 初始化设备树节点及节点携带的设备名称、类型数据。
    void initializeDeviceTree();

    // 设置当前高亮的功能切页。
    // @param button 需要激活的导航按钮，传 nullptr 时清除所有按钮的激活状态。
    void setActiveNav(QPushButton *button);

    Ui::wid_stack *ui;
    QStackedWidget *m_contentStack = nullptr;
    QTreeWidget *m_deviceTree = nullptr;
    WidReview *m_reviewPage = nullptr;
    wid_datasource *m_dataSourceWidget = nullptr;
    wid_customdatasource *m_customDataSourceWidget = nullptr;
    QPushButton *m_deviceNavButton = nullptr;
    QPushButton *m_dataSourceNavButton = nullptr;
    QPushButton *m_dataReviewNavButton = nullptr;
    QLabel *m_clockLabel = nullptr;
    QPushButton *m_openDeviceButton = nullptr;
    QPushButton *m_loginAuthButton = nullptr;
    QLineEdit *m_deviceNameEdit = nullptr;
    QLineEdit *m_deviceTypeEdit = nullptr;
    QLineEdit *m_deviceStatusEdit = nullptr;
    QLabel *m_fcInterfaceStatusLabel = nullptr;
    QLabel *m_auroraInterfaceStatusLabel = nullptr;
    QLabel *m_ethernetInterfaceStatusLabel = nullptr;
    QString m_currentDeviceName;
    QString m_currentDeviceType;
    QMap<QString, QTimer *> m_deviceStatusTimers;
    QMap<QString, QTreeWidgetItem *> m_deviceItems;
};
