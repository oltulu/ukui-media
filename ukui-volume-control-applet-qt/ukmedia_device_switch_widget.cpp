/*
 * Copyright (C) 2019 Tianjin KYLIN Information Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/&gt;.
 *
 */
#include "ukmedia_device_switch_widget.h"
extern "C" {
#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib/gi18n.h>
}
#include <QDebug>
extern "C" {
#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gobject/gparamspecs.h>
#include <glib/gi18n.h>
}
#include <XdgIcon>
#include <XdgDesktopFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStringList>
#include <QSpacerItem>
#include <QListView>
#include <QPainter>
#include <QScreen>
#include <QApplication>
#include <QSvgRenderer>
#include <QDebug>

typedef enum {
    DEVICE_VOLUME_BUTTON,
    APP_VOLUME_BUTTON
}ButtonType;

ButtonType btnType = DEVICE_VOLUME_BUTTON;
guint appnum = 0;
bool isShow = true;

UkmediaTrayIcon::UkmediaTrayIcon(QWidget *parent)
{
    Q_UNUSED(parent);
}

UkmediaTrayIcon::~UkmediaTrayIcon()
{

}

/*
    获取托盘图标的滚动事件
*/
bool UkmediaTrayIcon::event(QEvent *event)
{
    bool value = false;
    QWheelEvent *e = static_cast<QWheelEvent *>(event);
    if (event->type() == QEvent::Wheel) {
        if (e->delta() > 0) {
            value = true;
        }
        else if (e->delta() < 0) {
            value = false;
        }
        Q_EMIT  wheelRollEventSignal(value);
    }
    return QSystemTrayIcon::event(e);

}

/*
    显示window
*/
void DeviceSwitchWidget::showWindow()
{
    this->show();
    isShow = false;
}

/*
    隐藏window
*/
void DeviceSwitchWidget::hideWindow()
{
    this->hide();
    isShow = true;
}

/*
    右键菜单
*/
void DeviceSwitchWidget::showMenu(int x,int y)
{
    menu->setGeometry(x,y,250,84);
}

DeviceSwitchWidget::DeviceSwitchWidget(QWidget *parent) : QWidget (parent)
{
    appScrollWidget = new ScrollWitget(this);
    devScrollWidget = new ScrollWitget(this);
    devWidget = new UkmediaDeviceWidget(this);
    appWidget = new ApplicationVolumeWidget(this);

    devScrollWidget->area->setWidget(devWidget);
    appScrollWidget->area->setWidget(appWidget);

    output_stream_list = new QStringList;
    input_stream_list = new QStringList;
    device_name_list = new QStringList;
    device_display_name_list = new QStringList;
    stream_control_list = new QStringList;
    //初始化matemixer
    if (mate_mixer_init() == FALSE) {
        qDebug() << "libmatemixer initialization failed, exiting";
    }
    //创建context
    context = mate_mixer_context_new();
    mate_mixer_context_set_app_name (context,_("Ukui Volume Control App"));//设置app名
    mate_mixer_context_set_app_id(context, GVC_APPLET_DBUS_NAME);
    mate_mixer_context_set_app_version(context,VERSION);
    mate_mixer_context_set_app_icon(context,"ukuimedia-volume-control");
    //打开context
    if G_UNLIKELY (mate_mixer_context_open(context) == FALSE) {
        g_warning ("Failed to connect to a sound system**********************");
    }
    appWidget->setFixedSize(358,500);
    devWidget->setFixedSize(358,320);

    devWidget->move(42,0);
    appWidget->move(42,0);
    appScrollWidget->move(42,0);
    devScrollWidget->move(42,0);
    this->setFixedSize(400,320);

    devWidget->show();
    appWidget->hide();
    //添加托盘及菜单
    systemTrayMenuInit();

    deviceSwitchWidgetInit();
    context_set_property(this);
    appScrollWidget->area->widget()->adjustSize();
    devScrollWidget->area->widget()->adjustSize();
    g_signal_connect (G_OBJECT (context),
                     "notify::state",
                     G_CALLBACK (on_context_state_notify),
                     this);
    connect(deviceBtn,SIGNAL(clicked()),this,SLOT(device_button_clicked_slot()));
    connect(appVolumeBtn,SIGNAL(clicked()),this,SLOT(appvolume_button_clicked_slot()));

    this->setStyleSheet("QWidget{width:400px;"
                        "height:320px;"
                        "background:rgba(14,19,22,0.95);"
                        "border-radius:3px 3px 0px 0px;}");
    setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::Popup);
    setWindowOpacity(0.95);
}

void DeviceSwitchWidget::systemTrayMenuInit()
{
    QIcon icon;
    QString soundPreference;
    menu = new QMenu(this);
    soundSystemTrayIcon = new UkmediaTrayIcon(this);

    //为系统托盘图标添加菜单静音和声音首选项
    actionMute = new QWidgetAction(menu);
    actionMute->setCheckable(true);
    soundSystemTrayIcon->setToolTip(tr("Output volume control"));
    actionMute->setObjectName("outputActionMute");
    actionSoundPreference = new QWidgetAction(menu);
    actionSoundPreferenceWid = new QWidget();
    actionMuteWid = new QWidget();

    QHBoxLayout *hLayout;
    hLayout = new QHBoxLayout();

    muteCheckBox = new QCheckBox(actionMuteWid);
    muteCheckBox->setFixedSize(16,16);
    muteCheckBox->setFocusPolicy(Qt::NoFocus);
    muteLabel = new QLabel(tr("Mute(M)"),actionMuteWid);

    hLayout->addWidget(muteCheckBox);
    hLayout->addWidget(muteLabel);
    hLayout->setSpacing(10);

    muteCheckBox->setStyleSheet("QCheckBox{background:transparent;border:0px;}");
    muteLabel->setStyleSheet("QLabel{background:transparent;border:0px;}");
    actionMuteWid->setLayout(hLayout);
    actionMuteWid->setObjectName("muteWid");

    actionSoundPreference->setDefaultWidget(actionSoundPreferenceWid);
    actionMute->setDefaultWidget(actionMuteWid);
    //设置右键菜单
    menu->addAction(actionMute);

    menu->addAction(actionSoundPreference);
    menu->setFixedWidth(250);
    menu->setFixedHeight(84);

    init_widget_action(actionSoundPreferenceWid,"/usr/share/ukui-media/img/setting.svg",tr("Sound preference(S)"));
    init_widget_action(actionMuteWid,"","");
    menu->setObjectName("outputSoundMenu");
    soundSystemTrayIcon->setContextMenu(menu);

    menu->setWindowOpacity(0.95);

    soundSystemTrayIcon->setVisible(true);

    connect(soundSystemTrayIcon,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),\
            this,SLOT(activatedSystemTrayIconSlot(QSystemTrayIcon::ActivationReason)));

    menu->setStyleSheet("QMenu {background-color: rgba(8,10,12,90%);"
                        "border: 1px solid #626c6e;padding: 4px 2px 4px 2px;}"
                        "QMenu::item {font-size: 14px;color: #ffffff;"
                        "height: 36px;width: 246px;}");
}

/*
    激活声音托盘图标
*/
void DeviceSwitchWidget::activatedSystemTrayIconSlot(QSystemTrayIcon::ActivationReason reason)
{
    QRect rect;
    int localX ,availableWidth,totalWidth;
    int localY,availableHeight,totalHeight;
    rect = soundSystemTrayIcon->geometry();
    //屏幕可用宽高
    availableWidth = QGuiApplication::screens().at(0)->availableGeometry().width();
    availableHeight = QGuiApplication::screens().at(0)->availableGeometry().height();
    //总共宽高
    totalWidth =  QGuiApplication::screens().at(0)->size().width();
    totalHeight = QGuiApplication::screens().at(0)->size().height();
    //显示界面位置
    localY = availableHeight - this->height();
    localX = rect.x() - (this->width()/2 - rect.size().height()/2) ;
    switch(reason) {
    //鼠标中间键点击图标
    case QSystemTrayIcon::MiddleClick: {
        if (this->isHidden()) {
            if (!actionMute->isChecked()) {
                muteCheckBox->setChecked(true);
            }
            else {
                muteCheckBox->setChecked(false);
            }
            Q_EMIT mouse_middle_clicked_signal();
        }
        else {
            hideWindow();
        }
        break;
    }
    //鼠标左键点击图标
    case QSystemTrayIcon::Trigger: {
        if (isShow) {
            if (rect.x() > availableWidth/2 && rect.x()< availableWidth  && rect.y() > availableHeight) {

                this->setGeometry(localX,availableHeight-this->height(),400,320);
            }
            else if (rect.x() > availableWidth/2 && rect.x()< availableWidth && rect.y() < 40 ) {

                this->setGeometry(localX,totalHeight-availableHeight,400,320);
            }            else if (rect.x() < 40 && rect.y() > availableHeight/2 && rect.y()< availableHeight) {

                this->setGeometry(totalWidth-availableWidth,localY,400,320);//左
            }
            else if (rect.x() > availableWidth && rect.y() > availableHeight/2 && rect.y() < availableHeight) {

                this->setGeometry(localX,localY,400,320);
            }
            this->show();
            break;
        }
        else {
            this->hide();
            break;
        }
    }
    //鼠标左键双击图标
    case QSystemTrayIcon::DoubleClick: {
        hideWindow();
        break;
    }
    case QSystemTrayIcon::Context: {
        localX = rect.x();
        localY = availableHeight - 84;
        showMenu(localX,localY);
        break;
    }
    default:
        break;
    }
}

/*
    QWidgetAction 初始化
*/
void DeviceSwitchWidget::init_widget_action(QWidget *wid, QString iconstr, QString textstr)
{
    QString style="QWidget{background:transparent;border:0px;}\
            QWidget:hover{background-color:#34bed8ef;}\
            QWidget:pressed{background-color:#3a123456;}";

    QHBoxLayout* layout=new QHBoxLayout(wid);
    wid->setLayout(layout);
    wid->setFixedSize(244,36);
    wid->setStyleSheet(style);
    wid->setFocusPolicy(Qt::NoFocus);

    if(!iconstr.isEmpty()) {
        QLabel* labelicon=new QLabel(wid);
        QSvgRenderer* svg=new QSvgRenderer(wid);
        svg->load(iconstr);
        QPixmap* pixmap=new QPixmap(16,16);
        pixmap->fill(Qt::transparent);
        QPainter p(pixmap);
        svg->render(&p);
        labelicon->setPixmap(*pixmap);
        labelicon->setFixedSize(pixmap->size());
        labelicon->setAlignment(Qt::AlignCenter);
        labelicon->setStyleSheet("QLabel{background:transparent;border:0px;}");
        layout->addWidget(labelicon);
    }

    QLabel* labeltext=new QLabel(wid);
    labeltext->setStyleSheet("background:transparent;border:0px;color:#ffffff;font-size:14px;");
    QByteArray textbyte=textstr.toLocal8Bit();
    char* text=textbyte.data();
    labeltext->setText(tr(text));
    labeltext->adjustSize();
    layout->addWidget(labeltext);

    if(!iconstr.isEmpty()) {
        layout->setContentsMargins(10,0,wid->width()-16-labeltext->width()-20,0);
        layout->setSpacing(10);
    }
    else {
        layout->setContentsMargins(36,0,0,0);
    }
}

/*初始化主界面*/
void DeviceSwitchWidget::deviceSwitchWidgetInit()
{
    const QSize iconSize(16,16);
    QWidget *deviceWidget = new QWidget(this);
    deviceWidget->setFixedSize(42,320);

    deviceBtn = new QPushButton(deviceWidget);
    appVolumeBtn = new QPushButton(deviceWidget);

    deviceBtn->setFlat(true);
    appVolumeBtn->setFlat(true);
    deviceBtn->setFocusPolicy(Qt::NoFocus);
    appVolumeBtn->setFocusPolicy(Qt::NoFocus);
    deviceBtn->setFixedSize(36,36);
    appVolumeBtn->setFixedSize(36,36);

    deviceBtn->setIconSize(iconSize);
    appVolumeBtn->setIconSize(iconSize);

    deviceBtn->setIcon(QIcon("/usr/share/ukui-media/img/device.svg"));
    appVolumeBtn->setIcon(QIcon("/usr/share/ukui-media/img/application.svg"));

    deviceBtn->move(2,10);
    appVolumeBtn->move(2,57);

    //切换按钮设置tooltip
    deviceBtn->setToolTip(tr("Device Volume"));
    appVolumeBtn->setToolTip(tr("Application Volume"));

    switch(btnType) {
        case DEVICE_VOLUME_BUTTON:
        appVolumeBtn->setStyleSheet("QPushButton{background:transparent;border:0px;"
                                    "padding-left:0px;}"
                                    "QPushButton::pressed{background:rgba(61,107,229,1);"
                                    "border-radius:4px;padding-left:0px;}");
        deviceBtn->setStyleSheet("QPushButton{background:rgba(61,107,229,1);"
                                 "border-radius:4px;padding-left:0px;}");
        break;
    case APP_VOLUME_BUTTON:
        deviceBtn->setStyleSheet("QPushButton{background:transparent;border:0px;"
                                 "padding-left:0px;}"
                                 "QPushButton::pressed{background:rgba(61,107,229,1);"
                                 "border-radius:4px;padding-left:0px;}");
        appVolumeBtn->setStyleSheet("QPushButton{background:rgba(61,107,229,1);"
                                    "border-radius:4px;padding-left:0px;}");
        break;
    }

    deviceWidget->setStyleSheet("QWidget{ border-right: 1px solid rgba(255,255,255,0.08);}");
}

/*点击切换设备按钮对应的槽函数*/
void DeviceSwitchWidget::device_button_clicked_slot()
{
    btnType = DEVICE_VOLUME_BUTTON;
    appWidget->hide();
    appScrollWidget->hide();
    devScrollWidget->show();
    devWidget->show();

    appVolumeBtn->setStyleSheet("QPushButton{background:transparent;border:0px;"
                                "padding-left:0px;}"
                                "QPushButton::pressed{background:rgba(61,107,229,1);"
                                "border-radius:4px;}");
    deviceBtn->setStyleSheet("QPushButton{background:rgba(61,107,229,1);"
                             "border-radius:4px;}");
}

/*点击切换应用音量按钮对应的槽函数*/
void DeviceSwitchWidget::appvolume_button_clicked_slot()
{
    btnType = APP_VOLUME_BUTTON;
    appScrollWidget->show();
    devScrollWidget->hide();
    appWidget->show();
    devWidget->hide();
    //切换按钮样式
    deviceBtn->setStyleSheet("QPushButton{background:transparent;border:0px;"
                             "padding-left:0px;}"
                             "QPushButton::pressed{background:rgba(61,107,229,1);"
                             "border-radius:4px;}");
    appVolumeBtn->setStyleSheet("QPushButton{background:rgba(61,107,229,1);"
                             "border-radius:4px;}");
}

/*
 * context状态通知
*/
void DeviceSwitchWidget::on_context_state_notify (MateMixerContext *context,GParamSpec *pspec,DeviceSwitchWidget *w)
{
    Q_UNUSED(pspec);
    MateMixerState state = mate_mixer_context_get_state (context);
    list_device(w,context);
    if (state == MATE_MIXER_STATE_READY) {
        update_icon_output(w,context);
        update_icon_input(w,context);
    }
}

/*
    context 存储control增加
*/
void DeviceSwitchWidget::on_context_stored_control_added (MateMixerContext *context,const gchar *name,DeviceSwitchWidget *w)
{
    MateMixerStreamControl *control;
    MateMixerStreamControlMediaRole media_role;

    control = MATE_MIXER_STREAM_CONTROL (mate_mixer_context_get_stored_control (context, name));
    if (G_UNLIKELY (control == nullptr))
        return;

    media_role = mate_mixer_stream_control_get_media_role (control);

    if (media_role == MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_EVENT)
        bar_set_stream_control (w, control);
}


/*
    当其他设备插入时添加这个stream
*/
void DeviceSwitchWidget::on_context_stream_added (MateMixerContext *context,const gchar *name,DeviceSwitchWidget *w)
{
    MateMixerStream *stream;
    MateMixerDirection direction;
    stream = mate_mixer_context_get_stream (context, name);
    if (G_UNLIKELY (stream == nullptr))
        return;
    direction = mate_mixer_stream_get_direction (stream);

    /* If the newly added stream belongs to the currently selected device and
     * the test button is hidden, this stream may be the one to allow the
     * sound test and therefore we may need to enable the button */
    add_stream (w, stream,context);
}

/*
列出设备
*/
void DeviceSwitchWidget::list_device(DeviceSwitchWidget *w,MateMixerContext *context)
{
    const GList *list;

    list = mate_mixer_context_list_streams (context);

    while (list != nullptr) {
        add_stream (w, MATE_MIXER_STREAM (list->data),context);
        MateMixerStream *s = MATE_MIXER_STREAM(list->data);
        const gchar *stream_name = mate_mixer_stream_get_name(s);

        MateMixerDirection direction = mate_mixer_stream_get_direction(s);
        if (direction == MATE_MIXER_DIRECTION_OUTPUT) {
            w->output_stream_list->append(stream_name);
        }
        else if (direction == MATE_MIXER_DIRECTION_INPUT) {
            w->input_stream_list->append(stream_name);
        }
        list = list->next;
    }

    list = mate_mixer_context_list_devices (context);

    while (list != nullptr) {
        QString str =  mate_mixer_device_get_label(MATE_MIXER_DEVICE (list->data));

        const gchar *dis_name = mate_mixer_device_get_name(MATE_MIXER_DEVICE (list->data));
        w->device_name_list->append(dis_name);
        list = list->next;
    }

}

void DeviceSwitchWidget::add_stream (DeviceSwitchWidget *w, MateMixerStream *stream,MateMixerContext *context)
{
    const GList *controls;
//    gboolean is_default = FALSE;
    MateMixerDirection direction;

    direction = mate_mixer_stream_get_direction (stream);
    if (direction == MATE_MIXER_DIRECTION_INPUT) {
        MateMixerStream *input;
        input = mate_mixer_context_get_default_input_stream (context);
        if (stream == input) {
            bar_set_stream (w, stream);
//            is_default = TRUE;
        }
    }
    else if (direction == MATE_MIXER_DIRECTION_OUTPUT) {
        MateMixerStream        *output;
        MateMixerStreamControl *control;
        output = mate_mixer_context_get_default_output_stream (context);
        control = mate_mixer_stream_get_default_control (stream);

        if (stream == output) {
            update_output_settings(w,control);
            bar_set_stream (w, stream);
//            is_default = TRUE;
        }
    }

    controls = mate_mixer_stream_list_controls (stream);

    while (controls != nullptr) {
        MateMixerStreamControl    *control = MATE_MIXER_STREAM_CONTROL (controls->data);
        MateMixerStreamControlRole role;

        role = mate_mixer_stream_control_get_role (control);
        if (role == MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION) {
            add_application_control (w, control);
        }
        controls = controls->next;
    }

    // XXX find a way to disconnect when removed
    g_signal_connect (G_OBJECT (stream),
                      "control-added",
                      G_CALLBACK (on_stream_control_added),
                      w);
    g_signal_connect (G_OBJECT (stream),
                      "control-removed",
                      G_CALLBACK (on_stream_control_removed),
                      w);
}

/*
    添加应用音量控制
*/
void DeviceSwitchWidget::add_application_control (DeviceSwitchWidget *w, MateMixerStreamControl *control)
{
    MateMixerStream *stream;
    MateMixerStreamControlMediaRole media_role;
    MateMixerAppInfo *info;
    guint app_count;
    MateMixerDirection direction = MATE_MIXER_DIRECTION_UNKNOWN;
    const gchar *app_id;
    const gchar *app_name;
    const gchar *app_icon;
    appnum++;
    app_count = appnum;

    media_role = mate_mixer_stream_control_get_media_role (control);

    /* Add stream to the applications page, but make sure the stream qualifies
     * for the inclusion */
    info = mate_mixer_stream_control_get_app_info (control);
    if (info == nullptr)
        return;

    /* Skip streams with roles we don't care about */
    if (media_role == MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_EVENT ||
        media_role == MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_TEST ||
        media_role == MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_ABSTRACT ||
        media_role == MATE_MIXER_STREAM_CONTROL_MEDIA_ROLE_FILTER)
            return;

    app_id = mate_mixer_app_info_get_id (info);

    /* These applications may have associated streams because they do peak
     * level monitoring, skip these too */
    if (!g_strcmp0 (app_id, "org.mate.VolumeControl") ||
        !g_strcmp0 (app_id, "org.gnome.VolumeControl") ||
        !g_strcmp0 (app_id, "org.PulseAudio.pavucontrol"))
            return;

    QString app_icon_name = mate_mixer_app_info_get_icon(info);
    app_name = mate_mixer_app_info_get_name (info);
    //添加应用音量
    add_app_to_tableview(w,int(appnum),app_name,app_icon_name,control);

    if (app_name == nullptr)
        app_name = mate_mixer_stream_control_get_label (control);
    if (app_name == nullptr)
        app_name = mate_mixer_stream_control_get_name (control);
    if (G_UNLIKELY (app_name == nullptr))
        return;

    /* By default channel bars use speaker icons, use microphone icons
     * instead for recording applications */
    stream = mate_mixer_stream_control_get_stream (control);
    if (stream != nullptr)
        direction = mate_mixer_stream_get_direction (stream);

    if (direction == MATE_MIXER_DIRECTION_INPUT) {
    }
    app_icon = mate_mixer_app_info_get_icon (info);
    if (app_icon == nullptr) {
        if (direction == MATE_MIXER_DIRECTION_INPUT)
            app_icon = "audio-input-microphone";
        else
            app_icon = "applications-multimedia";
    }

    bar_set_stream_control (w, control);
}

void DeviceSwitchWidget::on_stream_control_added (MateMixerStream *stream,const gchar *name,DeviceSwitchWidget *w)
{
    MateMixerStreamControl    *control;
    MateMixerStreamControlRole role;
    w->stream_control_list->append(name);
    control = mate_mixer_stream_get_control (stream, name);
    if G_UNLIKELY (control == nullptr)
        return;

    role = mate_mixer_stream_control_get_role (control);
    if (role == MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION) {
        add_application_control (w, control);
    }
}

/*
    移除control
*/
void DeviceSwitchWidget::on_stream_control_removed (MateMixerStream *stream,const gchar *name,DeviceSwitchWidget *w)
{
    Q_UNUSED(stream);
    /* No way to be sure that it is an application control, but we don't have
     * any other than application bars that could match the name */
    remove_application_control (w, name);
}

void DeviceSwitchWidget::remove_application_control (DeviceSwitchWidget *w,const gchar *name)
{
    g_debug ("Removing application stream %s", name);
        /* We could call bar_set_stream_control here, but that would pointlessly
         * invalidate the channel bar, so just remove it ourselves */
    int i = w->stream_control_list->indexOf(name);

    w->stream_control_list->removeAt(i);

    //当播放音乐的应用程序退出后删除该项
    QLayoutItem *item = w->appWidget->gridlayout->takeAt(i);
    item->widget()->setParent(nullptr);
    delete  item;

    w->appWidget->gridlayout->update();
//    w->standItemModel->removeRow(i);
    if (appnum <= 0) {
        g_warn_if_reached ();
        appnum = 1;
    }
    appnum--;
    if (appnum <= 0)
        w->appWidget->noAppLabel->show();
    else
        w->appWidget->noAppLabel->hide();

}

void DeviceSwitchWidget::add_app_to_tableview(DeviceSwitchWidget *w,int appnum, const gchar *app_name,QString app_icon_name,MateMixerStreamControl *control)
{
    //设置QTableView每行的宽度
    //获取应用静音状态及音量
    int volume = 0;
    gboolean is_mute = false;
    gdouble normal = 0.0;
    is_mute = mate_mixer_stream_control_get_mute(control);
    volume = int(mate_mixer_stream_control_get_volume(control));
    normal = mate_mixer_stream_control_get_normal_volume(control);
    int display_volume = int(100 * volume / normal);

    //设置应用的图标
    QString iconName = "/usr/share/applications/";
    iconName.append(app_icon_name);
    iconName.append(".desktop");
    XdgDesktopFile xdg;
    xdg.load(iconName);
    QIcon i=xdg.icon();
    GError **error = nullptr;
    GKeyFileFlags flags = G_KEY_FILE_NONE;
    GKeyFile *keyflie = g_key_file_new();
    QByteArray fpbyte = iconName.toLocal8Bit();
//    char *filepath = "/usr/share/applications";//fpbyte.data();
    g_key_file_load_from_file(keyflie,iconName.toLocal8Bit(),flags,error);
    char *icon_str = g_key_file_get_locale_string(keyflie,"Desktop Entry","Icon",nullptr,nullptr);
    QIcon icon = QIcon::fromTheme(QString::fromLocal8Bit(icon_str));
    w->appWidget->app_volume_list->append(app_icon_name);

    //widget显示应用音量
    QWidget *app_widget = new QWidget(w->appWidget);
    app_widget->setFixedSize(304,52);
    QHBoxLayout *hlayout1 = new QHBoxLayout(app_widget);
    QVBoxLayout *vlayout = new QVBoxLayout();

    QWidget *wid1 = new QWidget(app_widget);
    QWidget *wid2 = new QWidget(app_widget);

    wid1->setFixedSize(254,22);
    wid2->setFixedSize(254,52);
    w->appWidget->appLabel = new QLabel(app_widget);
    w->appWidget->appIconBtn = new QPushButton(app_widget);
    w->appWidget->appIconLabel = new QLabel(app_widget);
    w->appWidget->appVolumeLabel = new QLabel(app_widget);
    w->appWidget->appSlider = new UkmediaDeviceSlider(app_widget);
    w->appWidget->appSlider->setOrientation(Qt::Horizontal);

    w->appWidget->appVolumeLabel->setFixedHeight(16);
    hlayout1->addWidget(w->appWidget->appSlider);
    hlayout1->addWidget(w->appWidget->appVolumeLabel);
    hlayout1->setSpacing(10);
    wid1->setLayout(hlayout1);
    wid1->layout()->setContentsMargins(0,0,0,0);

    vlayout->addWidget(w->appWidget->appLabel);
    vlayout->addWidget(wid1);
    vlayout->setSpacing(10);
    wid2->setLayout(vlayout);
    wid2->layout()->setContentsMargins(0,0,0,6);

    wid2->move(50,0);
    //添加widget到gridlayout中
    w->appWidget->gridlayout->addWidget(app_widget);
    w->appWidget->gridlayout->setMargin(0);

    //设置每项的固定大小
    w->appWidget->appLabel->setFixedSize(88,14);
    w->appWidget->appIconBtn->setFixedSize(32,32);
    w->appWidget->appIconLabel->setFixedSize(24,24);
    w->appWidget->appVolumeLabel->setFixedSize(24,14);

    QSize icon_size(32,32);
    w->appWidget->appIconBtn->setIconSize(icon_size);
    w->appWidget->appIconBtn->setStyleSheet("QPushButton{background:transparent;border:0px;padding-left:0px;}");
    w->appWidget->appIconBtn->setIcon(icon);
//    w->appWidget->appIconBtn->setFlat(true);
//    w->appWidget->appIconBtn->setFocusPolicy(Qt::NoFocus);
    w->appWidget->appIconBtn->setEnabled(true);

    w->appWidget->appSlider->setMaximum(100);
    w->appWidget->appSlider->setFixedSize(220,20);

    QString appSliderStr = app_name;
    QString appLabelStr = app_name;
    QString appVolumeLabelStr = app_name;

    appSliderStr.append("Slider");
    appLabelStr.append("Label");
    appVolumeLabelStr.append("VolumeLabel");
    w->appWidget->appSlider->setObjectName(appSliderStr);
    w->appWidget->appLabel->setObjectName(appLabelStr);
    w->appWidget->appVolumeLabel->setObjectName(appVolumeLabelStr);
    //设置label 和滑动条的值
    w->appWidget->appLabel->setText(app_name);
    w->appWidget->appSlider->setValue(display_volume);
    w->appWidget->appVolumeLabel->setNum(display_volume);

    /*滑动条控制应用音量*/
    connect(w->appWidget->appSlider,&QSlider::valueChanged,[=](int value){
        QSlider *s = w->findChild<QSlider*>(appSliderStr);
        s->setValue(value);
        QLabel *l = w->findChild<QLabel*>(appVolumeLabelStr);
        l->setNum(value);

        int v = int(value*65536/100 + 0.5);
        mate_mixer_stream_control_set_volume(control,guint(v));
    });
    /*应用音量同步*/
    g_signal_connect (G_OBJECT (control),
                     "notify::volume",
                     G_CALLBACK (update_app_volume),
                     w);

    connect(w,&DeviceSwitchWidget::app_volume_changed,[=](bool is_mute,int volume,const gchar *app_name){
        Q_UNUSED(is_mute);
        QString slider_str = app_name;
        slider_str.append("Slider");
        QSlider *s = w->findChild<QSlider*>(slider_str);
        s->setValue(volume);
    });

    if (appnum <= 0)
        w->appWidget->noAppLabel->show();
    else
        w->appWidget->noAppLabel->hide();

    //设置布局的垂直间距以及设置gridlayout四周的间距
    w->appWidget->gridlayout->setContentsMargins(0,44,30,w->appWidget->height() - 72 - appnum * 67);
    w->appWidget->appLabel->setStyleSheet("QLabel{width:210px;"
                                          "height:14px;"
                                          "font-size:14px;"
                                          "color:rgba(255,255,255,0.57);"
                                          "line-height:28px;}");
}

/*
    同步应用音量
*/
void DeviceSwitchWidget::update_app_volume(MateMixerStreamControl *control, GParamSpec *pspec, DeviceSwitchWidget *w)
{
    Q_UNUSED(pspec);

    guint value = mate_mixer_stream_control_get_volume(control);
    guint volume ;
    volume = guint(value*100/65536.0+0.5);
    bool is_mute = mate_mixer_stream_control_get_mute(control);
    MateMixerAppInfo *info = mate_mixer_stream_control_get_app_info(control);
    const gchar *app_name = mate_mixer_app_info_get_name(info);
    Q_EMIT w->app_volume_changed(is_mute,int(volume),app_name);

}

/*
    连接context，处理不同信号
*/
void DeviceSwitchWidget::set_context(DeviceSwitchWidget *w,MateMixerContext *context)
{
    g_signal_connect (G_OBJECT (context),
                      "stream-added",
                      G_CALLBACK (on_context_stream_added),
                      w);

    g_signal_connect (G_OBJECT (context),
                    "stream-removed",
                    G_CALLBACK (on_context_stream_removed),
                    w);

    g_signal_connect (G_OBJECT (context),
                    "device-added",
                    G_CALLBACK (on_context_device_added),
                    w);
    g_signal_connect (G_OBJECT (context),
                    "device-removed",
                    G_CALLBACK (on_context_device_removed),
                    w);

    g_signal_connect (G_OBJECT (context),
                    "notify::default-input-stream",
                    G_CALLBACK (on_context_default_input_stream_notify),
                    w);
    g_signal_connect (G_OBJECT (context),
                    "notify::default-output-stream",
                    G_CALLBACK (on_context_default_output_stream_notify),
                    w);

    g_signal_connect (G_OBJECT (context),
                    "stored-control-added",
                    G_CALLBACK (on_context_stored_control_added),
                    w);
    g_signal_connect (G_OBJECT (context),
                    "stored-control-removed",
                    G_CALLBACK (on_context_stored_control_removed),
                    w);

}

/*
    remove stream
*/
void DeviceSwitchWidget::on_context_stream_removed (MateMixerContext *context,const gchar *name,DeviceSwitchWidget *w)
{
    Q_UNUSED(context);
    remove_stream (w, name);
}

/*
    移除stream
*/
void DeviceSwitchWidget::remove_stream (DeviceSwitchWidget *w, const gchar *name)
{
    MateMixerStream *stream = mate_mixer_context_get_stream(w->context,name);
    MateMixerDirection direction = mate_mixer_stream_get_direction(stream);
    bool status;
    if (direction == MATE_MIXER_DIRECTION_INPUT) {
        status = w->input_stream_list->removeOne(name);
    }
    else if (direction == MATE_MIXER_DIRECTION_OUTPUT) {
        status = w->output_stream_list->removeOne(name);
    }
    if (w->appWidget->app_volume_list != nullptr) {
        bar_set_stream (w,nullptr);
    }
}

/*
    context 添加设备并设置到单选框
*/
void DeviceSwitchWidget::on_context_device_added (MateMixerContext *context, const gchar *name, DeviceSwitchWidget *w)
{
    MateMixerDevice *device;
    device = mate_mixer_context_get_device (context, name);
    if (G_UNLIKELY (device == nullptr))
        return;
    add_device (w, device);
}

/*
    添加设备
*/
void DeviceSwitchWidget::add_device (DeviceSwitchWidget *w, MateMixerDevice *device)
{
    const gchar *name;
    const gchar *label;

    name  = mate_mixer_device_get_name (device);
    label = mate_mixer_device_get_label (device);
    w->device_name_list->append(name);
}

/*
    移除设备
*/
void DeviceSwitchWidget::on_context_device_removed (MateMixerContext *context,const gchar *name,DeviceSwitchWidget *w)
{
    int  count = 0;
    MateMixerDevice *dev = mate_mixer_context_get_device(context,name);
    QString str = mate_mixer_device_get_label(dev);
    do {
        if (name == w->device_name_list->at(count)) {
            w->device_name_list->removeAt(count);
            break;
        }
        count++;
        if (count > w->device_name_list->size()) {
            break;
        }
    }while(1);
}

/*
    默认输入流通知
*/
void DeviceSwitchWidget::on_context_default_input_stream_notify (MateMixerContext *context,GParamSpec *pspec,DeviceSwitchWidget *w)
{
    Q_UNUSED(pspec);
    MateMixerStream *stream;

    g_debug ("Default input stream has changed");
    stream = mate_mixer_context_get_default_input_stream (context);
    set_input_stream (w, stream);
}

void DeviceSwitchWidget::set_input_stream (DeviceSwitchWidget *w, MateMixerStream *stream)
{

    bar_set_stream (w, stream);

    if (stream != nullptr) {
        const GList *controls;

        controls = mate_mixer_context_list_stored_controls (w->context);

        /* Move all stored controls to the newly selected default stream */
        while (controls != nullptr) {
            MateMixerStream *parent;

            MateMixerStreamControl *control = MATE_MIXER_STREAM_CONTROL (controls->data);
            parent  = mate_mixer_stream_control_get_stream (control);

            /* Prefer streamless controls to stay the way they are, forcing them to
             * a particular owning stream would be wrong for eg. event controls */
            if (parent != nullptr && parent != stream) {
                MateMixerDirection direction =mate_mixer_stream_get_direction (parent);

                if (direction == MATE_MIXER_DIRECTION_INPUT)
                    mate_mixer_stream_control_set_stream (control, stream);
            }
            controls = controls->next;
        }

        /* Enable/disable the peak level monitor according to mute state */
        g_signal_connect (G_OBJECT (stream),
                          "notify::mute",
                          G_CALLBACK (on_stream_control_mute_notify),
                          w);
    }

}

/*
    control 静音通知
*/
void DeviceSwitchWidget::on_stream_control_mute_notify (MateMixerStreamControl *control,GParamSpec *pspec,DeviceSwitchWidget *w)
{
    Q_UNUSED(pspec);
    Q_UNUSED(w);
    /* Stop monitoring the input stream when it gets muted */
    if (mate_mixer_stream_control_get_mute (control) == TRUE)
        mate_mixer_stream_control_set_monitor_enabled (control, FALSE);
    else
        mate_mixer_stream_control_set_monitor_enabled (control, TRUE);
}

/*
    默认输出流通知
*/
void DeviceSwitchWidget::on_context_default_output_stream_notify (MateMixerContext *context,GParamSpec *pspec,DeviceSwitchWidget *w)
{
    MateMixerStream *stream;
    stream = mate_mixer_context_get_default_output_stream (context);
//    update_icon_output(w,context);
    set_output_stream (w, stream);
}

/*
    移除存储control
*/
void DeviceSwitchWidget::on_context_stored_control_removed (MateMixerContext *context,const gchar *name,DeviceSwitchWidget *w)
{
    Q_UNUSED(context);
    Q_UNUSED(name);
    if (w->appWidget->app_volume_list != nullptr) {
        bar_set_stream (w, nullptr);
    }
}

/*
 * context设置属性
*/
void DeviceSwitchWidget::context_set_property(DeviceSwitchWidget *w)
{
    set_context(w,w->context);
}

/*
    更新输入音量及图标
*/
void DeviceSwitchWidget::update_icon_input (DeviceSwitchWidget *w,MateMixerContext *context)
{
    MateMixerStream        *stream;
    MateMixerStreamControl *control = nullptr;
    const gchar *app_id;
    gboolean show = FALSE;

    stream = mate_mixer_context_get_default_input_stream (context);

    const GList *inputs =mate_mixer_stream_list_controls(stream);
    control = mate_mixer_stream_get_default_control(stream);

    //初始化滑动条的值
    int volume = int(mate_mixer_stream_control_get_volume(control));
    int value = int(volume *100 /65536.0+0.5);
    w->devWidget->inputDeviceSlider->setValue(value);
    QString percent = QString::number(value);
    w->devWidget->inputVolumeLabel->setText(percent);

    while (inputs != nullptr) {
        MateMixerStreamControl *input = MATE_MIXER_STREAM_CONTROL (inputs->data);
        MateMixerStreamControlRole role = mate_mixer_stream_control_get_role (input);
        if (role == MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION) {
            MateMixerAppInfo *app_info = mate_mixer_stream_control_get_app_info (input);
            app_id = mate_mixer_app_info_get_id (app_info);
            if (app_id == nullptr) {
                /* A recording application which has no
                 * identifier set */
                g_debug ("Found a recording application control %s",
                    mate_mixer_stream_control_get_label (input));

                if G_UNLIKELY (control == nullptr) {
                    /* In the unlikely case when there is no
                     * default input control, use the application
                     * control for the icon */
                    control = input;
                }
                show = TRUE;
                break;
            }

            if (strcmp (app_id, "org.mate.VolumeControl") != 0 &&
                strcmp (app_id, "org.gnome.VolumeControl") != 0 &&
                strcmp (app_id, "org.PulseAudio.pavucontrol") != 0) {

                g_debug ("Found a recording application %s", app_id);
                if G_UNLIKELY (control == nullptr)
                    control = input;
                show = TRUE;
                break;
            }
        }
        inputs = inputs->next;
    }

    if (show == TRUE)
            g_debug ("Input icon enabled");
    else
            g_debug ("There is no recording application, input icon disabled");

    connect(w->devWidget->inputDeviceSlider,&QSlider::valueChanged,[=](int value){
        QString percent;

        percent = QString::number(value);
        mate_mixer_stream_control_set_mute(control,FALSE);
        int volume = value*65536/100;
        gboolean ok = mate_mixer_stream_control_set_volume(control,volume);
        w->devWidget->inputVolumeLabel->setText(percent);
    });
    gvc_stream_status_icon_set_control (w, control);

    if (control != nullptr) {
            g_debug ("Output icon enabled");
    }
    else {
            g_debug ("There is no output stream/control, output icon disabled");
    }
    if(show) {
        w->devWidget->inputWidgetShow();
    }

}

/*
    更新输出音量及图标
*/
void DeviceSwitchWidget::update_icon_output (DeviceSwitchWidget *w,MateMixerContext *context)
{
    MateMixerStream *stream;
    MateMixerStreamControl *control = nullptr;

    stream = mate_mixer_context_get_default_output_stream (context);
    if (stream != nullptr)
        control = mate_mixer_stream_get_default_control (stream);

    gvc_stream_status_icon_set_control (w, control);
    //初始化滑动条的值
    bool state = mate_mixer_stream_control_get_mute(control);
    int volume = int(mate_mixer_stream_control_get_volume(control));
    int value = int(volume *100 /65536.0+0.5);
    w->devWidget->outputDeviceSlider->setValue(value);
    QString percent = QString::number(value);

    QString systemTrayIcon;
    QIcon icon;
    if (state || value <= 0) {
        systemTrayIcon = "audio-volume-muted";
        icon = QIcon::fromTheme(systemTrayIcon);
        w->soundSystemTrayIcon->setIcon(QIcon(icon));
        w->muteCheckBox->setChecked(true);
    }
    else if (value > 0 && value <= 33) {
        systemTrayIcon = "audio-volume-low";
        icon = QIcon::fromTheme(systemTrayIcon);
        w->soundSystemTrayIcon->setIcon(QIcon(icon));
        w->muteCheckBox->setChecked(false);
    }
    else if(value > 33 && value <= 66) {
        systemTrayIcon = "audio-volume-medium";
        icon = QIcon::fromTheme(systemTrayIcon);
        w->soundSystemTrayIcon->setIcon(QIcon(icon));
        w->muteCheckBox->setChecked(false);
    }
    else if (value > 66) {
        systemTrayIcon = "audio-volume-high";
        icon = QIcon::fromTheme(systemTrayIcon);
        w->soundSystemTrayIcon->setIcon(QIcon(icon));
        w->muteCheckBox->setChecked(false);
    }
    w->devWidget->outputVolumeLabel->setText(percent);

    //输出音量控制
    //输出滑动条和音量控制
    connect(w->devWidget->outputDeviceSlider,&QSlider::valueChanged,[=](int value){
        QString percent;

        percent = QString::number(value);
        mate_mixer_stream_control_set_mute(control,FALSE);
        int volume = value*65536/100;
        mate_mixer_stream_control_set_volume(control,guint(volume));
        w->devWidget->outputVolumeLabel->setText(percent);
    });

    connect(w->muteCheckBox,&QCheckBox::released,[=](){
        int volume = int(mate_mixer_stream_control_get_volume(control));
        volume = int(volume*100/65536.0 + 0.5);
        bool status = mate_mixer_stream_control_get_mute(control);
        if (status) {
            status = false;
            w->muteCheckBox->setChecked(status);
            mate_mixer_stream_control_set_mute(control,status);
            w->updateSystemTrayIcon(volume,status);
        }
        else {
            status =true;
            w->muteCheckBox->setChecked(status);
            mate_mixer_stream_control_set_mute(control,status);
            w->updateSystemTrayIcon(volume,status);
        }
        w->menu->hide();
    });

    //静音action点击
    connect(w->actionMute,&QWidgetAction::triggered,[=](bool isMute){
        isMute = mate_mixer_stream_control_get_mute(control);
        int opVolume = int(mate_mixer_stream_control_get_volume(control));
        opVolume = int(opVolume*100/65536.0 + 0.5);
        if (isMute) {
            w->devWidget->outputVolumeLabel->setNum(opVolume);
            mate_mixer_stream_control_set_mute(control,FALSE);
        }
        else {
            mate_mixer_stream_control_set_mute(control,TRUE);
        }
        isMute = mate_mixer_stream_control_get_mute(control);
        int volume = int(mate_mixer_stream_control_get_volume(control));
        volume = int(volume*100/65536.0+0.5);
        w->updateSystemTrayIcon(volume,isMute);
    });

    //鼠标中间健点击托盘图标
    connect(w,&DeviceSwitchWidget::mouse_middle_clicked_signal,[=](){
        bool isMute = mate_mixer_stream_control_get_mute(control);
        int opVolume = int(mate_mixer_stream_control_get_volume(control));
        opVolume = int(opVolume*100/65536.0 + 0.5);
        if (isMute) {
            w->devWidget->outputVolumeLabel->setNum(opVolume);
            mate_mixer_stream_control_set_mute(control,FALSE);
        }
        else {
            mate_mixer_stream_control_set_mute(control,TRUE);
        }
        isMute = mate_mixer_stream_control_get_mute(control);
        int volume = int(mate_mixer_stream_control_get_volume(control));
        volume = int(volume*100/65536.0+0.5);
        w->updateSystemTrayIcon(volume,isMute);
    });

    //鼠标滚轮滚动托盘图标
    connect(w->soundSystemTrayIcon,&UkmediaTrayIcon::wheelRollEventSignal,[=](bool step){
        int volume = int(mate_mixer_stream_control_get_volume(control));
        volume = int(volume*100/65536.0+0.5);
        if (step) {
            w->devWidget->outputDeviceSlider->setValue(volume+5);
        }
        else {
            w->devWidget->outputDeviceSlider->setValue(volume-5);
        }
    });
    //当widget显示时鼠标滚轮控制声音
    connect(w,&DeviceSwitchWidget::mouse_wheel_signal,[=](bool step){
        int volume = int(mate_mixer_stream_control_get_volume(control));
        volume = int(volume*100/65536.0+0.5);
        if (step) {
            w->devWidget->outputDeviceSlider->setValue(volume+5);
        }
        else {
            w->devWidget->outputDeviceSlider->setValue(volume-5);
        }
    });
    if (control != nullptr) {
            g_debug ("Output icon enabled");
    }
    else {
            g_debug ("There is no output stream/control, output icon disabled");
    }
}

void DeviceSwitchWidget::gvc_stream_status_icon_set_control (DeviceSwitchWidget *w,MateMixerStreamControl *control)
{
    g_signal_connect ( G_OBJECT (control),
                      "notify::volume",
                      G_CALLBACK (on_stream_control_volume_notify),
                      w);
    g_signal_connect (G_OBJECT (control),
                      "notify::mute",
                      G_CALLBACK (on_stream_control_mute_notify),
                      w);

    MateMixerDirection direction = mate_mixer_stored_control_get_direction((MateMixerStoredControl *)control);
    if (direction == MATE_MIXER_DIRECTION_OUTPUT) {
        MateMixerStreamControlFlags flags = mate_mixer_stream_control_get_flags(control);
        if (flags & MATE_MIXER_STREAM_CONTROL_MUTE_READABLE) {
            g_signal_connect (G_OBJECT (control),
                              "notify::mute",
                              G_CALLBACK (on_control_mute_notify),
                              w);
        }
    }
}

/*
    静音通知
*/
void DeviceSwitchWidget::on_control_mute_notify (MateMixerStreamControl *control,GParamSpec *pspec,DeviceSwitchWidget *w)
{
    Q_UNUSED(pspec);
    gboolean mute = mate_mixer_stream_control_get_mute (control);
    int volume = int(mate_mixer_stream_control_get_volume(control));
    volume = int(volume*100/65536.0+0.5);
    w->updateSystemTrayIcon(volume,mute);
}

/*
    stream control 声音通知
*/
void DeviceSwitchWidget::on_stream_control_volume_notify (MateMixerStreamControl *control,GParamSpec *pspec,DeviceSwitchWidget *w)
{
    Q_UNUSED(pspec);
    MateMixerStreamControlFlags flags;
    gboolean muted = FALSE;
    gdouble decibel = 0.0;
    guint volume = 0;

    QString decscription;

    if (control != nullptr)
        flags = mate_mixer_stream_control_get_flags(control);

    if(flags&MATE_MIXER_STREAM_CONTROL_MUTE_READABLE)
        muted = mate_mixer_stream_control_get_mute(control);

    if (flags&MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE) {
        volume = mate_mixer_stream_control_get_volume(control);
    }

    if (flags&MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL)
        decibel = mate_mixer_stream_control_get_decibel(control);
    decscription = mate_mixer_stream_control_get_label(control);

    MateMixerStream *stream = mate_mixer_stream_control_get_stream(control);
    MateMixerDirection direction = mate_mixer_stream_get_direction(stream);

    //设置输出滑动条的值
    int value = int(volume*100/65536.0 + 0.5);
    if (direction == MATE_MIXER_DIRECTION_OUTPUT) {
        w->devWidget->outputDeviceSlider->setValue(value);
        w->updateSystemTrayIcon(value,muted);
    }
    else if (direction == MATE_MIXER_DIRECTION_INPUT) {
        w->devWidget->inputDeviceSlider->setValue(value);
    }
}


/*
    更新输出设置
*/
void DeviceSwitchWidget::update_output_settings (DeviceSwitchWidget *w,MateMixerStreamControl *control)
{
    Q_UNUSED(w);
    MateMixerStreamControlFlags flags;
    flags = mate_mixer_stream_control_get_flags(control);

    if (flags & MATE_MIXER_STREAM_CONTROL_CAN_BALANCE) {

    }

}

void DeviceSwitchWidget::set_output_stream (DeviceSwitchWidget *w, MateMixerStream *stream)
{
    MateMixerStreamControl *control;
    bar_set_stream (w,stream);

    if (stream != nullptr) {
        const GList *controls;

        controls = mate_mixer_context_list_stored_controls (w->context);

        /* Move all stored controls to the newly selected default stream */
        while (controls != nullptr) {
            MateMixerStream        *parent;
            MateMixerStreamControl *control;

            control = MATE_MIXER_STREAM_CONTROL (controls->data);
            parent  = mate_mixer_stream_control_get_stream (control);

            /* Prefer streamless controls to stay the way they are, forcing them to
             * a particular owning stream would be wrong for eg. event controls */
            if (parent != nullptr && parent != stream) {
                    MateMixerDirection direction =
                            mate_mixer_stream_get_direction (parent);

                    if (direction == MATE_MIXER_DIRECTION_OUTPUT)
                            mate_mixer_stream_control_set_stream (control, stream);
            }
            controls = controls->next;
        }
    }

//        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
    update_output_stream_list (w, stream);
    update_output_settings(w,control);
}

/*
    更新输出stream 列表
*/
void DeviceSwitchWidget::update_output_stream_list(DeviceSwitchWidget *w,MateMixerStream *stream)
{
    const gchar *name = nullptr;
    if (stream != nullptr) {
        name = mate_mixer_stream_get_name(stream);
        w->output_stream_list->append(name);
    }
}

/*
    bar设置stream
*/
void DeviceSwitchWidget::bar_set_stream (DeviceSwitchWidget  *w,MateMixerStream *stream)
{
    MateMixerStreamControl *control = nullptr;

    if (stream != nullptr)
        control = mate_mixer_stream_get_default_control (stream);

    bar_set_stream_control (w, control);
}

void DeviceSwitchWidget::bar_set_stream_control (DeviceSwitchWidget *w,MateMixerStreamControl *control)
{
    Q_UNUSED(w);
    const gchar *name;
    if (control != nullptr) {
        name = mate_mixer_stream_control_get_name (control);
    }
}

/*
    点击窗口之外的部分隐藏
*/
bool DeviceSwitchWidget:: event(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (QApplication::activeWindow() != this) {
            hideWindow();
        }
    }
    return QWidget::event(event);
}



/*
    滚轮滚动事件
*/
void DeviceSwitchWidget::wheelEvent(QWheelEvent *event)
{
    bool step;
    if (event->delta() >0 ) {
        step = true;
    }
    else if (event->delta() < 0 ) {
        step = false;
    }
    Q_EMIT mouse_wheel_signal(step);
    event->accept();
}

void DeviceSwitchWidget::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event);
    hideWindow();
}

/*
    按键事件,控制系统音量
*/
void DeviceSwitchWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hideWindow();
    }
}

/*
    更新声音托盘图标
*/
void DeviceSwitchWidget::updateSystemTrayIcon(int volume,bool isMute)
{
    QString systemTrayIcon;
    QIcon icon;
    if (isMute) {
        systemTrayIcon = "audio-volume-muted";
        icon = QIcon::fromTheme(systemTrayIcon);
        muteCheckBox->setChecked(true);
        soundSystemTrayIcon->setIcon(icon);
    }
    else if (volume <= 0) {
        systemTrayIcon = "audio-volume-muted";
        icon = QIcon::fromTheme(systemTrayIcon);
        muteCheckBox->setChecked(true);
        soundSystemTrayIcon->setIcon(icon);
    }
    else if (volume > 0 && volume <= 33) {
        systemTrayIcon = "audio-volume-low";
        muteCheckBox->setChecked(false);
        icon = QIcon::fromTheme(systemTrayIcon);
        soundSystemTrayIcon->setIcon(icon);
    }
    else if (volume >33 && volume <= 66) {
        systemTrayIcon = "audio-volume-medium";
        muteCheckBox->setChecked(false);
        icon = QIcon::fromTheme(systemTrayIcon);
        soundSystemTrayIcon->setIcon(icon);
    }
    else {
        systemTrayIcon = "audio-volume-high";
        muteCheckBox->setChecked(false);
        icon = QIcon::fromTheme(systemTrayIcon);
        soundSystemTrayIcon->setIcon(icon);
    }

    //设置声音菜单栏静音选项的勾选状态
    if (isMute) {
        muteCheckBox->setChecked(true);
    }
    else {
        muteCheckBox->setChecked(false);
    }
}

DeviceSwitchWidget::~DeviceSwitchWidget()
{

}
