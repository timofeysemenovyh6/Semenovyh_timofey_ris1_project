#include "MainWindow.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <functional>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QEnterEvent>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>
#include <QTextBrowser>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPixmap>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

static QString optToText(const QJsonValue& v) {
  if (v.isNull() || v.isUndefined()) return {};
  if (v.isString()) return v.toString();
  if (v.isDouble()) return QString::number(v.toDouble(), 'f', 0);
  if (v.isBool()) return v.toBool() ? "true" : "false";
  return {};
}

namespace {
constexpr int kHome = 0;
constexpr int kWizard = 1;
constexpr int kChat = 2;
constexpr int kCompare = 3;
constexpr int kFavorites = 4;
constexpr int kLab = 5;
constexpr int kAdmin = 6;

/// История в QScrollArea: у QTextEdit sizeHint растёт с текстом и выталкивает поле ввода за экран.
class ChatHistoryView : public QTextEdit {
 public:
  explicit ChatHistoryView(QWidget* parent = nullptr) : QTextEdit(parent) {
    setReadOnly(true);
    setAcceptRichText(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::NoFocus);
    document()->setDocumentMargin(10);
  }
  QSize minimumSizeHint() const override { return QSize(0, 72); }
  QSize sizeHint() const override { return QSize(400, 160); }
};
}  // namespace

static QPixmap loadLandingPixmapFromDisk() {
  const QStringList tries = {QStringLiteral(":/assets/landing-bg.png"),
                             QCoreApplication::applicationDirPath() + QStringLiteral("/../static/landing-bg.png"),
                             QCoreApplication::applicationDirPath() + QStringLiteral("/../../static/landing-bg.png"),
                             QCoreApplication::applicationDirPath() + QStringLiteral("/../../../static/landing-bg.png")};
  for (const QString& p : tries) {
    if (!QFile::exists(p)) continue;
    QPixmap pm;
    if (pm.load(p)) return pm;
  }
  return {};
}

static QPixmap loadSidebarLogoPixmap() {
  const QStringList tries = {QStringLiteral(":/assets/logo.png"),
                             QCoreApplication::applicationDirPath() + QStringLiteral("/../static/logo.png"),
                             QCoreApplication::applicationDirPath() + QStringLiteral("/../../static/logo.png"),
                             QCoreApplication::applicationDirPath() + QStringLiteral("/../../../static/logo.png")};
  for (const QString& p : tries) {
    if (p.startsWith(QStringLiteral(":/")) || QFile::exists(p)) {
      QPixmap pm;
      if (pm.load(p)) return pm;
    }
  }
  return {};
}

static QPixmap roundedSidebarLogo(const QPixmap& src, int size = 40, int radius = 12) {
  if (src.isNull()) return {};
  QPixmap out(size, size);
  out.fill(Qt::transparent);
  QPainter painter(&out);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  QPainterPath clip;
  clip.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
  painter.setClipPath(clip);
  const QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  painter.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
  return out;
}

class SidebarHeaderClickFilter final : public QObject {
 public:
  explicit SidebarHeaderClickFilter(std::function<void()> handler, QObject* parent = nullptr)
      : QObject(parent), handler_(std::move(handler)) {}

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    Q_UNUSED(watched);
    if (event->type() == QEvent::MouseButtonRelease) {
      const auto* me = static_cast<const QMouseEvent*>(event);
      if (me->button() == Qt::LeftButton && handler_) {
        handler_();
        return true;
      }
    }
    return false;
  }

 private:
  std::function<void()> handler_;
};

// Фон hero как .lHero::before в style.css (cover, center right + горизонтальный градиент).
static QRect landingBgTargetRect(const QRect& viewport, const QSize& imgSize) {
  if (imgSize.isEmpty()) return viewport;
  const qreal scale =
      qMax(qreal(viewport.width()) / qreal(imgSize.width()), qreal(viewport.height()) / qreal(imgSize.height()));
  const int tw = int(qreal(imgSize.width()) * scale);
  const int th = int(qreal(imgSize.height()) * scale);
  const int x = viewport.right() - tw + 1;
  const int y = viewport.top() + (viewport.height() - th) / 2;
  return QRect(x, y, tw, th);
}

static void paintLandingHeroBackground(QPainter& painter, const QRect& r, const QPixmap& bg) {
  painter.fillRect(r, QColor(6, 16, 31));

  if (!bg.isNull()) {
    painter.save();
    painter.setClipRect(r);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRect target = landingBgTargetRect(r, bg.size());
    painter.drawPixmap(target, bg);
    painter.restore();
  }

  QLinearGradient veil(r.topLeft(), QPoint(r.right(), r.top()));
  veil.setColorAt(0.0, QColor(6, 16, 31, 235));
  veil.setColorAt(0.44, QColor(6, 16, 31, 189));
  veil.setColorAt(0.72, QColor(6, 16, 31, 46));
  veil.setColorAt(1.0, QColor(6, 16, 31, 10));
  painter.fillRect(r, veil);
}

class LandingHeroWidget final : public QWidget {
 public:
  explicit LandingHeroWidget(QWidget* parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
    bg_ = loadLandingPixmapFromDisk();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(22, 18, 22, 18);
    lay->setSpacing(0);
    content_ = new QWidget(this);
    content_->setAttribute(Qt::WA_TranslucentBackground, true);
    content_->setAutoFillBackground(false);
    // Layout для content_ задаётся в setupHomePage() — здесь второй layout ломал отображение кнопок.
    lay->addStretch(1);
    lay->addWidget(content_, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lay->addStretch(1);
  }

  QWidget* contentWidget() const { return content_; }

  void setBackgroundPixmap(QPixmap pm) {
    if (!pm.isNull()) {
      bg_ = std::move(pm);
      update();
    }
  }

 protected:
  void paintEvent(QPaintEvent* e) override {
    Q_UNUSED(e);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    paintLandingHeroBackground(painter, rect(), bg_);
  }

 private:
  QWidget* content_ = nullptr;
  QPixmap bg_;
};

static void paintPlayCircle(QPainter& painter, const QRect& r) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(QColor(255, 255, 255, 89), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(r.adjusted(0, 0, -1, -1));
  const int cx = r.center().x();
  const int cy = r.center().y();
  QPolygonF tri;
  tri << QPointF(cx - 3.0, cy - 5.0) << QPointF(cx - 3.0, cy + 5.0) << QPointF(cx + 5.0, cy);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(234, 240, 255, 242));
  painter.drawPolygon(tri);
  painter.restore();
}

// «Подобрать автомобиль» — .landingV2 .btn.btn--solid.btn--lg
class LandingSolidCta final : public QWidget {
 public:
  explicit LandingSolidCta(const QString& label, QWidget* parent = nullptr) : QWidget(parent), label_(label) {
    setObjectName(QStringLiteral("LandingSolidCta"));
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setMinimumHeight(50);
    setMaximumHeight(50);
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    QFont f = font();
    f.setPixelSize(15);
    f.setWeight(QFont::DemiBold);
    setFont(f);
    setMinimumWidth(QFontMetrics(f).horizontalAdvance(label_) + 52);
  }

  std::function<void()> onClicked;

 protected:
  void paintEvent(QPaintEvent* e) override {
    Q_UNUSED(e);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect box = rect().adjusted(1, 1, -1, -1);
    if (pressed_) box = box.adjusted(0, 1, 0, -1);

    QLinearGradient grad(box.topLeft(), box.bottomRight());
    if (hovered_) {
      grad.setColorAt(0.0, QColor(0x48, 0xa5, 0xff));
      grad.setColorAt(0.5, QColor(0x20, 0x80, 0xee));
      grad.setColorAt(1.0, QColor(0x18, 0x68, 0xcf));
    } else {
      grad.setColorAt(0.0, QColor(0x3b, 0x9f, 0xff));
      grad.setColorAt(0.5, QColor(0x1e, 0x74, 0xe8));
      grad.setColorAt(1.0, QColor(0x15, 0x5c, 0xc4));
    }
    painter.setPen(QPen(QColor(60, 150, 255, 128), 2));
    painter.setBrush(grad);
    painter.drawRoundedRect(box, 10, 10);

    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, label_);
  }

  void enterEvent(QEnterEvent* e) override {
    QWidget::enterEvent(e);
    hovered_ = true;
    update();
  }

  void leaveEvent(QEvent* e) override {
    QWidget::leaveEvent(e);
    hovered_ = false;
    pressed_ = false;
    update();
  }

  void mousePressEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton) {
      pressed_ = true;
      update();
    }
    QWidget::mousePressEvent(e);
  }

  void mouseReleaseEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton) {
      const bool hit = rect().contains(e->position().toPoint());
      pressed_ = false;
      update();
      if (hit && onClicked) onClicked();
    }
    QWidget::mouseReleaseEvent(e);
  }

 private:
  QString label_;
  bool hovered_ = false;
  bool pressed_ = false;
};

// «Как мы работаем» — .btn.btn--ghost.btn--lg + .lPlay
class LandingGhostCta final : public QWidget {
 public:
  explicit LandingGhostCta(const QString& label, QWidget* parent = nullptr) : QWidget(parent), label_(label) {
    setObjectName(QStringLiteral("LandingGhostCta"));
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setMinimumHeight(50);
    setMaximumHeight(50);
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    QFont f = font();
    f.setPixelSize(15);
    f.setWeight(QFont::DemiBold);
    setFont(f);
    const int textW = QFontMetrics(f).horizontalAdvance(label_);
    setMinimumWidth(28 + 10 + textW + 4);
  }

  std::function<void()> onClicked;

 protected:
  void paintEvent(QPaintEvent* e) override {
    Q_UNUSED(e);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    if (hovered_) painter.fillRect(r, QColor(255, 255, 255, 18));

    constexpr int kPlay = 28;
    const int playY = (r.height() - kPlay) / 2;
    paintPlayCircle(painter, QRect(0, playY, kPlay, kPlay));

    painter.setPen(QColor(234, 240, 255, hovered_ ? 255 : 235));
    const QRect textRect(kPlay + 10, 0, r.width() - kPlay - 10, r.height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, label_);
  }

  void enterEvent(QEnterEvent* e) override {
    QWidget::enterEvent(e);
    hovered_ = true;
    update();
  }

  void leaveEvent(QEvent* e) override {
    QWidget::leaveEvent(e);
    hovered_ = false;
    update();
  }

  void mouseReleaseEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClicked) onClicked();
    QWidget::mouseReleaseEvent(e);
  }

 private:
  QString label_;
  bool hovered_ = false;
};

class MainColumnBackdrop final : public QWidget {
 public:
  explicit MainColumnBackdrop(QWidget* parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    bg_ = loadLandingPixmapFromDisk();
  }

  void setBackgroundPixmap(QPixmap pm) {
    if (!pm.isNull()) {
      bg_ = std::move(pm);
      update();
    }
  }

 protected:
  void paintEvent(QPaintEvent* e) override {
    Q_UNUSED(e);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    QLinearGradient base(r.topLeft(), QPoint(r.left(), r.bottom()));
    base.setColorAt(0, QColor(7, 10, 18));
    base.setColorAt(1, QColor(11, 18, 38));
    painter.fillRect(r, base);

    painter.setPen(Qt::NoPen);
    {
      QRadialGradient g1(r.width() * 0.1f, r.height() * 0.02f, r.width() * 0.55f);
      g1.setColorAt(0, QColor(37, 208, 185, 51));
      g1.setColorAt(1, Qt::transparent);
      painter.setBrush(g1);
      painter.drawRect(r);
    }
    {
      QRadialGradient g2(r.width() * 0.9f, r.height() * 0.12f, r.width() * 0.5f);
      g2.setColorAt(0, QColor(255, 77, 141, 46));
      g2.setColorAt(1, Qt::transparent);
      painter.setBrush(g2);
      painter.drawRect(r);
    }
    {
      QRadialGradient g3(r.width() * 0.5f, r.height() * 0.98f, r.height() * 0.55f);
      g3.setColorAt(0, QColor(124, 92, 255, 51));
      g3.setColorAt(1, Qt::transparent);
      painter.setBrush(g3);
      painter.drawRect(r);
    }

    if (!bg_.isNull()) {
      painter.save();
      painter.setOpacity(0.22f);
      const QSize img = bg_.size();
      const qreal scale = qMax(qreal(r.width()) / qreal(img.width()), qreal(r.height()) / qreal(img.height()));
      const int tw = int(qreal(img.width()) * scale);
      const int th = int(qreal(img.height()) * scale);
      const QRect target(r.width() - tw, (r.height() - th) / 2, tw, th);
      painter.drawPixmap(target, bg_);
      painter.restore();
    }

    QLinearGradient veil(r.topLeft(), r.bottomLeft());
    veil.setColorAt(0, QColor(7, 10, 18, 168));
    veil.setColorAt(1, QColor(7, 10, 18, 188));
    painter.fillRect(r, veil);
  }

 private:
  QPixmap bg_;
};

static void prepareStackPage(QWidget* page) {
  page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

static QFrame* makeCard(QWidget* parent) {
  auto* f = new QFrame(parent);
  f->setObjectName(QStringLiteral("AppCard"));
  f->setFrameShape(QFrame::StyledPanel);
  return f;
}

static QFrame* makeLandingSection(QWidget* parent, const QString& title, const QString& body,
                                  const QString& anchorObjectName = {}) {
  auto* f = new QFrame(parent);
  if (anchorObjectName.isEmpty())
    f->setObjectName(QStringLiteral("LandingSection"));
  else
    f->setObjectName(anchorObjectName);
  auto* lay = new QVBoxLayout(f);
  lay->setContentsMargins(22, 18, 22, 22);
  lay->setSpacing(8);
  auto* t = new QLabel(title, f);
  t->setObjectName(QStringLiteral("SectionTitle"));
  auto* p = new QLabel(body, f);
  p->setObjectName(QStringLiteral("PageLead"));
  p->setWordWrap(true);
  lay->addWidget(t);
  lay->addWidget(p);
  return f;
}

static void appendChatHtml(QTextEdit* w, const QString& html) {
  w->moveCursor(QTextCursor::End);
  w->insertHtml(html + QStringLiteral("<br><br>"));
  w->moveCursor(QTextCursor::End);
  if (auto* bar = w->verticalScrollBar()) bar->setValue(bar->maximum());
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  net_ = new QNetworkAccessManager(this);
  net_->setCookieJar(new QNetworkCookieJar(net_));
  setupUi();
  applyAppStyle();
  refreshSidebarLogo();
  fetchLandingBackground();
  fetchSidebarLogo();
  setWindowTitle(QStringLiteral("AutoSelect — подбор автомобилей"));
  resize(1080, 760);
}

void MainWindow::applyAppStyle() {
  // Цвета и типографика в духе static/style.css (:root + .sidebar + .card + .chatBar).
  setStyleSheet(QStringLiteral(
      "QMainWindow { background-color: #070A12; }"
      "QWidget#mainContentWrap { background: transparent; }"
      "QWidget#StackPage { background: transparent; }"
      "QScrollArea#MainScroll { background: transparent; border: none; }"
      "QScrollArea#MainScroll > QWidget > QWidget { background: transparent; }"
      "QFrame#LandingSection, QFrame#secServices, QFrame#secHow, QFrame#secReviews, QFrame#secAbout {"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(255,255,255,8), stop:1 rgba(255,255,255,3));"
      "  border-top: 1px solid rgba(255,255,255,20);"
      "  border-radius: 0;"
      "}"
      "QLabel#SectionTitle { font-size: 18px; font-weight: 700; color: #F4F7FF; margin: 0; letter-spacing: -0.1px; }"
      "QWidget#SidebarTop {"
      "  background: transparent;"
      "  border-bottom: 1px solid rgba(255,255,255,20);"
      "}"
      "QWidget#SidebarTop:hover { background: rgba(255,255,255,10); }"
      "QPushButton#NavWeb {"
      "  text-align: left;"
      "  padding: 12px 14px;"
      "  border-radius: 10px;"
      "  font-size: 14px;"
      "  font-weight: 600;"
      "  color: rgba(244,247,255,225);"
      "  border: 1px solid transparent;"
      "  background: transparent;"
      "}"
      "QPushButton#NavWeb:hover {"
      "  background: rgba(255,255,255,38);"
      "  border-color: rgba(255,255,255,51);"
      "  color: #ffffff;"
      "}"
      "QPushButton#NavMinor {"
      "  text-align: left;"
      "  padding: 9px 14px;"
      "  border-radius: 10px;"
      "  font-size: 13px;"
      "  font-weight: 500;"
      "  color: rgba(183,194,240,209);"
      "  border: 1px solid transparent;"
      "  background: transparent;"
      "}"
      "QPushButton#NavMinor:hover { color: rgba(244,247,255,242); background: rgba(255,255,255,20); }"
      "QScrollArea#SideNavScroll { background: transparent; border: none; }"
      "QScrollArea#SideNavScroll > QWidget > QWidget { background: transparent; }"
      "QWidget#SidebarFoot { border-top: 1px solid rgba(255,255,255,26); background: transparent; }"
      "QPushButton#SideContactBtn {"
      "  min-height: 42px;"
      "  border-radius: 9px;"
      "  font-size: 14px;"
      "  font-weight: 600;"
      "  color: rgba(244,247,255,240);"
      "  border: 2px solid rgba(255,255,255,56);"
      "  background: rgba(255,255,255,8);"
      "  padding: 0 12px;"
      "}"
      "QPushButton#SideContactBtn:hover {"
      "  border-color: rgba(37,208,185,128);"
      "  background: rgba(37,208,185,20);"
      "}"
      "QWidget#LandingHero { background: transparent; }"
      "QLabel#LandingH1Line {"
      "  font-size: 52px; font-weight: 700; color: #F4F7FF;"
      "  letter-spacing: -0.6px; padding: 0; margin: 0;"
      "  min-height: 60px;"
      "}"
      "QLabel#LandingH1Accent {"
      "  font-size: 52px; font-weight: 700; color: #2d8cff;"
      "  letter-spacing: -0.6px; padding: 0; margin: 0;"
      "  min-height: 60px;"
      "}"
      "QWidget#LandingTitleBlock { background: transparent; }"
      "QLabel#LandingLead { font-size: 14px; color: rgba(234,240,255,199); margin: 0; }"
      "QWidget#LandingProp { background: transparent; }"
      "QLabel#LandingPropIcon {"
      "  min-width: 34px; max-width: 34px; min-height: 34px; max-height: 34px;"
      "  border-radius: 17px;"
      "  border: 1px solid rgba(255,255,255,51);"
      "  background: rgba(0,0,0,51);"
      "  font-size: 15px;"
      "  color: rgba(234,240,255,220);"
      "  qproperty-alignment: AlignCenter;"
      "}"
      "QLabel#LandingPropText { font-size: 13px; color: rgba(234,240,255,219); }"
      "QWidget#LandingSolidCta, QWidget#LandingGhostCta { background: transparent; border: none; }"
      "QWidget#CentralRoot { background-color: #070A12; }"
      "QWidget#Sidebar {"
      "  min-width: 260px; max-width: 280px;"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(4,8,16,245), stop:1 rgba(6,12,24,235));"
      "  border-right: 1px solid rgba(255,255,255,26);"
      "}"
      "QLabel#SideLogoName { font-size: 17px; font-weight: 800; color: #F4F7FF; letter-spacing: -0.3px; }"
      "QLabel#SideLogoSub { font-size: 12px; color: rgba(183,194,240,220); }"
      "QLabel#SideMeta { font-size: 11px; color: rgba(183,194,240,140); letter-spacing: 0.04em; }"
      "QLabel#MainFooter { font-size: 13px; color: rgba(183,194,240,190); padding: 10px 8px 14px 8px; "
      "  border-top: 1px solid rgba(255,255,255,26); }"
      "QWidget#SideNav { background: transparent; }"
      "QPushButton#NavBtn {"
      "  text-align: left;"
      "  padding: 12px 14px;"
      "  border-radius: 10px;"
      "  font-size: 14px;"
      "  font-weight: 600;"
      "  color: rgba(244,247,255,225);"
      "  border: 1px solid transparent;"
      "  background: transparent;"
      "}"
      "QPushButton#NavBtn:hover {"
      "  background: rgba(255,255,255,38);"
      "  border-color: rgba(255,255,255,51);"
      "  color: #ffffff;"
      "}"
      "QPushButton#NavBtn:checked {"
      "  background: rgba(37,208,185,31);"
      "  border-color: rgba(37,208,185,71);"
      "  color: #e8fffb;"
      "}"
      "QLabel#LogoMark {"
      "  min-width: 40px; max-width: 40px; min-height: 40px; max-height: 40px;"
      "  border-radius: 12px;"
      "  border: 1px solid rgba(255,255,255,36);"
      "  background: transparent;"
      "}"
      "QFrame#AppCard {"
      "  border-radius: 18px;"
      "  border: 1px solid rgba(255,255,255,31);"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(255,255,255,23), stop:1 rgba(255,255,255,13));"
      "}"
      "QLabel#PageTitle { font-size: 30px; font-weight: 700; color: #F4F7FF; margin-bottom: 4px; }"
      "QLabel#PageLead, QLabel#FormHint { font-size: 14px; color: rgba(183,194,235,235); line-height: 1.45; }"
      "QLabel#HeroAccent { color: #2d8cff; }"
      "QFormLayout QLabel { color: rgba(183,194,240,235); font-size: 13px; }"
      "QLineEdit {"
      "  padding: 10px 12px;"
      "  border-radius: 11px;"
      "  border: 1.5px solid rgba(255,255,255,41);"
      "  background: rgba(4,8,18,140);"
      "  color: #F4F7FF;"
      "  font-size: 15px;"
      "  selection-background-color: rgba(37,208,185,120);"
      "}"
      "QLineEdit:hover { border-color: rgba(255,255,255,56); background: rgba(4,8,18,158); }"
      "QLineEdit:focus {"
      "  border-color: rgba(37,208,185,140);"
      "  background: rgba(4,10,20,184);"
      "}"
      "QTextEdit {"
      "  border-radius: 14px;"
      "  border: 1px solid rgba(255,255,255,31);"
      "  background: rgba(0,0,0,46);"
      "  color: #F4F7FF;"
      "  font-size: 14px;"
      "  padding: 8px;"
      "  selection-background-color: rgba(37,208,185,120);"
      "}"
      "QTableWidget {"
      "  gridline-color: rgba(255,255,255,26);"
      "  background: rgba(0,0,0,31);"
      "  alternate-background-color: rgba(255,255,255,10);"
      "  color: #F4F7FF;"
      "  border: none;"
      "  border-radius: 12px;"
      "}"
      "QTableWidget::item { padding: 6px; }"
      "QHeaderView::section {"
      "  background: rgba(255,255,255,10);"
      "  color: rgba(183,194,240,235);"
      "  font-weight: 700;"
      "  font-size: 13px;"
      "  padding: 8px;"
      "  border: none;"
      "  border-bottom: 1px solid rgba(255,255,255,26);"
      "}"
      "QPushButton#PrimaryBtn {"
      "  min-height: 44px;"
      "  padding: 0 22px;"
      "  border-radius: 10px;"
      "  font-size: 14px;"
      "  font-weight: 600;"
      "  color: #ffffff;"
      "  border: 2px solid rgba(60,150,255,128);"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3b9fff, stop:0.5 #1e74e8, stop:1 #155cc4);"
      "}"
      "QPushButton#PrimaryBtn:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4aa8ff, stop:1 #1a66d6); }"
      "QPushButton#PrimaryBtn:pressed { padding-top: 2px; }"
      "QPushButton#OutlineBtn {"
      "  min-height: 40px;"
      "  padding: 0 16px;"
      "  border-radius: 9px;"
      "  font-size: 13px;"
      "  font-weight: 600;"
      "  color: rgba(244,247,255,240);"
      "  border: 2px solid rgba(255,255,255,56);"
      "  background: rgba(255,255,255,8);"
      "}"
      "QPushButton#OutlineBtn:hover {"
      "  border-color: rgba(37,208,185,128);"
      "  background: rgba(37,208,185,20);"
      "}"
      "QPushButton#DangerBtn {"
      "  min-height: 40px;"
      "  border-radius: 9px;"
      "  font-weight: 600;"
      "  color: #ffc9c9;"
      "  border: 2px solid rgba(255,94,94,107);"
      "  background: rgba(255,94,94,15);"
      "}"
      "QPushButton#DangerBtn:hover { background: rgba(255,94,94,31); }"
      "QFrame#ChatBar {"
      "  border-radius: 12px;"
      "  border: 1.5px solid rgba(255,255,255,41);"
      "  background: rgba(4,8,18,115);"
      "}"
      "QFrame#ChatBar QLineEdit {"
      "  border: none; background: transparent; color: #F4F7FF;"
      "  font-size: 15px; padding: 10px 14px; min-height: 22px;"
      "}"
      "QPushButton#ChatSend {"
      "  min-height: 48px;"
      "  border-radius: 0;"
      "  border: none;"
      "  border-left: 1px solid rgba(255,255,255,31);"
      "  font-weight: 600;"
      "  color: #041218;"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2ae8cf, stop:1 #129988);"
      "}"
      "QPushButton#ChatSend:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3cf0d8, stop:1 #14a896); }"
      "QPushButton#HeartBtn {"
      "  min-width: 36px; min-height: 34px;"
      "  border-radius: 10px;"
      "  border: 1px solid rgba(255,255,255,36);"
      "  background: rgba(4,8,18,90);"
      "  font-size: 18px;"
      "  color: rgba(255,77,141,200);"
      "}"
      "QPushButton#HeartBtn:hover {"
      "  border-color: rgba(255,77,141,90);"
      "  color: rgba(255,77,141,217);"
      "  background: rgba(255,77,141,20);"
      "}"
      "QScrollBar:vertical { width: 10px; background: rgba(0,0,0,40); margin: 0; }"
      "QScrollBar::handle:vertical { background: rgba(255,255,255,51); min-height: 28px; border-radius: 5px; }"
      "QScrollBar:horizontal { height: 10px; background: rgba(0,0,0,40); }"
      "QScrollBar::handle:horizontal { background: rgba(255,255,255,51); min-width: 28px; border-radius: 5px; }"
      "QFrame#BlindCard {"
      "  border-radius: 12px;"
      "  border: 1px solid rgba(255,255,255,36);"
      "  background: rgba(0,0,0,46);"
      "}"
      "QFrame#BlindCardPicked {"
      "  border-radius: 12px;"
      "  border: 2px solid rgba(37,208,185,140);"
      "  background: rgba(37,208,185,18);"
      "}"
      "QLabel#BlindMark {"
      "  min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px;"
      "  border-radius: 10px;"
      "  font-size: 15px; font-weight: 800; color: #e8e4ff;"
      "  background: rgba(124,92,255,64);"
      "  border: 1px solid rgba(124,92,255,102);"
      "  qproperty-alignment: AlignCenter;"
      "}"
      "QLabel#BlindReveal { font-size: 14px; font-weight: 700; color: #F4F7FF; }"
      "QLabel#BlindStats { font-size: 14px; color: rgba(207,214,238,245); line-height: 1.55; }"
      "QPushButton#BlindPickBtn {"
      "  min-height: 44px;"
      "  border-radius: 9px;"
      "  font-size: 14px;"
      "  font-weight: 600;"
      "  color: #041218;"
      "  border: none;"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2ae8cf, stop:1 #129988);"
      "}"
      "QPushButton#BlindPickBtn:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3cf0d8, stop:1 #14a896); }"
      "QPushButton#BlindPickBtn:disabled {"
      "  color: rgba(183,194,240,160);"
      "  background: rgba(255,255,255,20);"
      "}"));
}

void MainWindow::setupUi() {
  auto* root = new QWidget(this);
  root->setObjectName(QStringLiteral("CentralRoot"));
  auto* outer = new QHBoxLayout(root);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  auto* sidebar = new QWidget(root);
  sidebar->setObjectName(QStringLiteral("Sidebar"));
  auto* sideLay = new QVBoxLayout(sidebar);
  sideLay->setContentsMargins(0, 0, 0, 0);
  sideLay->setSpacing(0);

  auto* sidebarTop = new QWidget(sidebar);
  sidebarTop->setObjectName(QStringLiteral("SidebarTop"));
  sidebarTop->setMinimumHeight(80);
  sidebarTop->setMaximumHeight(80);
  sidebarTop->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  sidebarTop->setCursor(Qt::PointingHandCursor);
  auto* topLay = new QHBoxLayout(sidebarTop);
  topLay->setContentsMargins(20, 22, 20, 16);
  topLay->setSpacing(12);
  logoMark_ = new QLabel(sidebarTop);
  logoMark_->setObjectName(QStringLiteral("LogoMark"));
  logoMark_->setFixedSize(40, 40);
  logoMark_->setScaledContents(false);
  auto* textCol = new QWidget(sidebarTop);
  auto* textLay = new QVBoxLayout(textCol);
  textLay->setContentsMargins(0, 0, 0, 0);
  textLay->setSpacing(2);
  auto* name = new QLabel(QStringLiteral("AutoSelect"), textCol);
  name->setObjectName(QStringLiteral("SideLogoName"));
  auto* sub = new QLabel(QStringLiteral("подбор автомобилей"), textCol);
  sub->setObjectName(QStringLiteral("SideLogoSub"));
  textLay->addWidget(name);
  textLay->addWidget(sub);
  topLay->addWidget(logoMark_, 0, Qt::AlignVCenter);
  topLay->addWidget(textCol, 1, Qt::AlignVCenter);
  auto* headerClick = new SidebarHeaderClickFilter([this]() { goToStackPage(kHome); }, sidebarTop);
  sidebarTop->installEventFilter(headerClick);

  auto* navHost = new QWidget(sidebar);
  navHost->setObjectName(QStringLiteral("SideNav"));
  auto* navLay = new QVBoxLayout(navHost);
  navLay->setContentsMargins(12, 16, 12, 12);
  navLay->setSpacing(4);

  navGroup_ = new QButtonGroup(this);
  navGroup_->setExclusive(true);

  auto addStackNav = [&](const QString& label, int stackId) {
    auto* b = new QPushButton(label, navHost);
    b->setObjectName(QStringLiteral("NavBtn"));
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    navLay->addWidget(b);
    navGroup_->addButton(b, stackId);
  };

  addStackNav(QStringLiteral("Главная"), kHome);
  addStackNav(QStringLiteral("Мастер‑подбор"), kWizard);
  addStackNav(QStringLiteral("Чат‑помощник"), kChat);
  addStackNav(QStringLiteral("Сравнение"), kCompare);
  addStackNav(QStringLiteral("Избранное"), kFavorites);
  addStackNav(QStringLiteral("Лаборатория"), kLab);
  addStackNav(QStringLiteral("База авто"), kAdmin);

  navLay->addStretch(1);

  auto* navScroll = new QScrollArea(sidebar);
  navScroll->setObjectName(QStringLiteral("SideNavScroll"));
  navScroll->setWidgetResizable(true);
  navScroll->setFrameShape(QFrame::NoFrame);
  navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  navScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  navHost->setObjectName(QStringLiteral("SideNav"));
  navScroll->setWidget(navHost);

  auto* foot = new QWidget(sidebar);
  foot->setObjectName(QStringLiteral("SidebarFoot"));
  foot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* footLay = new QVBoxLayout(foot);
  footLay->setContentsMargins(16, 12, 16, 18);
  footLay->setSpacing(8);
  auto* contact = new QPushButton(QStringLiteral("Связаться с нами"), foot);
  contact->setObjectName(QStringLiteral("SideContactBtn"));
  contact->setCursor(Qt::PointingHandCursor);
  contact->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(contact, &QPushButton::clicked, this, [this]() { goToStackPage(kChat); });
  footLay->addWidget(contact);
  auto* meta = new QLabel(QStringLiteral("C++ · SQLite · локально"), foot);
  meta->setObjectName(QStringLiteral("SideMeta"));
  meta->setWordWrap(true);
  meta->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  footLay->addWidget(meta);

  sideLay->addWidget(sidebarTop);
  sideLay->addWidget(navScroll, 1);
  sideLay->addWidget(foot, 0);

  auto* mainCol = new QWidget(root);
  auto* mainColOuter = new QVBoxLayout(mainCol);
  mainColOuter->setContentsMargins(0, 0, 0, 0);
  mainColOuter->setSpacing(0);

  auto* plate = new QWidget(mainCol);
  auto* grid = new QGridLayout(plate);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(0);

  mainBackdrop_ = new MainColumnBackdrop(plate);
  auto* contentWrap = new QWidget(plate);
  contentWrap->setObjectName(QStringLiteral("mainContentWrap"));
  contentWrap->setAttribute(Qt::WA_TranslucentBackground, true);
  auto* cwLay = new QVBoxLayout(contentWrap);
  cwLay->setContentsMargins(0, 0, 0, 0);
  cwLay->setSpacing(0);

  stack_ = new QStackedWidget(contentWrap);
  cwLay->addWidget(stack_, 1);

  mainFooter_ = new QLabel(QStringLiteral("Локальный сервис подбора автомобилей."), contentWrap);
  mainFooter_->setObjectName(QStringLiteral("MainFooter"));
  mainFooter_->setWordWrap(true);
  cwLay->addWidget(mainFooter_);

  grid->addWidget(mainBackdrop_, 0, 0);
  grid->addWidget(contentWrap, 0, 0);
  mainBackdrop_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  contentWrap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  mainColOuter->addWidget(plate, 1);

  outer->addWidget(sidebar);
  outer->addWidget(mainCol, 1);

  setupHomePage();
  setupWizardTab();
  setupChatTab();
  setupCompareTab();
  setupFavoritesTab();
  setupLabTab();
  setupAdminTab();

  {
    auto* sepNavMid = new QFrame(navHost);
    sepNavMid->setFrameShape(QFrame::HLine);
    sepNavMid->setMaximumHeight(1);
    sepNavMid->setStyleSheet(QStringLiteral("background: rgba(255,255,255,26); margin: 10px 8px 12px 8px; border: none;"));
    const int ins = navLay->count() - 1;
    navLay->insertWidget(ins, sepNavMid);
    auto addMinorScroll = [&](const QString& label, QFrame* target) {
      auto* b = new QPushButton(label, navHost);
      b->setObjectName(QStringLiteral("NavMinor"));
      b->setFlat(true);
      b->setCursor(Qt::PointingHandCursor);
      connect(b, &QPushButton::clicked, this, [this, target]() { scrollHomeTo(target); });
      navLay->insertWidget(navLay->count() - 1, b);
    };
    addMinorScroll(QStringLiteral("Услуги"), secServices_);
    addMinorScroll(QStringLiteral("Этапы работы"), secHow_);
    addMinorScroll(QStringLiteral("Отзывы"), secReviews_);
    addMinorScroll(QStringLiteral("О компании"), secAbout_);
  }

  connect(navGroup_, &QButtonGroup::idClicked, this, &MainWindow::onNavPage);

  if (mainBackdrop_) mainBackdrop_->setVisible(false);
  if (mainFooter_) mainFooter_->setVisible(true);

  goToStackPage(kHome);

  setCentralWidget(root);
}

void MainWindow::onNavPage(int id) {
  goToStackPage(id);
}

void MainWindow::goToStackPage(int stackIndex) {
  {
    QSignalBlocker block(navGroup_);
    if (auto* b = navGroup_->button(stackIndex)) b->setChecked(true);
  }
  stack_->setCurrentIndex(stackIndex);
  if (mainBackdrop_) mainBackdrop_->setVisible(stackIndex != kHome);
  if (mainFooter_) mainFooter_->setVisible(stackIndex == kHome);
  reloadPageData(stackIndex);
  if (stackIndex == kHome) updateHomeHeroHeight();
  if (stackIndex == kChat && chatInput_) {
    QTimer::singleShot(0, chatInput_, [this]() {
      if (chatInput_) chatInput_->setFocus(Qt::OtherFocusReason);
    });
  }
}

void MainWindow::reloadPageData(int stackIndex) {
  if (stackIndex == kFavorites) getJson(QStringLiteral("/api/favorites"));
  else if (stackIndex == kCompare)
    getJson(QStringLiteral("/api/compare"));
  else if (stackIndex == kWizard)
    getJson(QStringLiteral("/api/my-car"));
  else if (stackIndex == kLab)
    getJson(QStringLiteral("/api/extras"));
  else if (stackIndex == kAdmin)
    getJson(QStringLiteral("/api/admin/cars"));
}

void MainWindow::scrollHomeTo(QWidget* w) {
  goToStackPage(kHome);
  if (!homeScroll_ || !w) return;
  QTimer::singleShot(80, this, [this, w]() {
    if (homeScroll_ && w) homeScroll_->ensureWidgetVisible(w);
  });
}

void MainWindow::refreshSidebarLogo() {
  if (!logoMark_) return;
  const QPixmap logo = roundedSidebarLogo(loadSidebarLogoPixmap());
  if (!logo.isNull()) logoMark_->setPixmap(logo);
}

void MainWindow::fetchSidebarLogo() {
  if (!logoMark_) return;
  QUrl base(baseUrl_);
  if (!base.isValid() || base.scheme().isEmpty()) return;
  const QUrl url = base.resolved(QUrl(QStringLiteral("/static/logo.png")));
  QNetworkRequest req(url);
  QNetworkReply* reply = net_->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;
    QPixmap pm;
    if (!pm.loadFromData(reply->readAll())) return;
    const QPixmap logo = roundedSidebarLogo(pm);
    if (!logo.isNull()) logoMark_->setPixmap(logo);
  });
}

void MainWindow::fetchLandingBackground() { tryFetchLandingBg(0); }

void MainWindow::tryFetchLandingBg(int attempt) {
  if (!mainBackdrop_ && !homeHero_) return;
  QUrl base(baseUrl_);
  if (!base.isValid() || base.scheme().isEmpty()) return;
  const QUrl url = base.resolved(QUrl(QStringLiteral("/static/landing-bg.png")));
  QNetworkRequest req(url);
  QNetworkReply* reply = net_->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, attempt]() {
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
      QPixmap pm;
      if (pm.loadFromData(reply->readAll())) {
        if (mainBackdrop_) mainBackdrop_->setBackgroundPixmap(pm);
        if (homeHero_) homeHero_->setBackgroundPixmap(std::move(pm));
        return;
      }
    }
    if (attempt < 15) {
      QTimer::singleShot(500, this, [this, attempt]() { tryFetchLandingBg(attempt + 1); });
    }
  });
}

void MainWindow::updateHomeHeroHeight() {
  if (!homeHero_ || !stack_) return;
  const int h = qMax(520, stack_->height());
  homeHero_->setMinimumHeight(h);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  updateHomeHeroHeight();
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  QTimer::singleShot(0, this, [this]() {
    updateHomeHeroHeight();
    if (logoMark_ && logoMark_->pixmap().isNull()) refreshSidebarLogo();
  });
}

void MainWindow::setupHomePage() {
  auto* scroll = new QScrollArea(stack_);
  scroll->setObjectName(QStringLiteral("MainScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* inner = new QWidget(scroll);
  inner->setObjectName(QStringLiteral("StackPage"));
  inner->setAttribute(Qt::WA_TranslucentBackground, true);
  scroll->setWidget(inner);

  auto* outer = new QVBoxLayout(inner);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  homeHero_ = new LandingHeroWidget(inner);
  homeHero_->setObjectName(QStringLiteral("LandingHero"));
  homeHero_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  auto* heroContent = homeHero_->contentWidget();
  heroContent->setMaximumWidth(560);
  QVBoxLayout* heroLay = qobject_cast<QVBoxLayout*>(heroContent->layout());
  if (!heroLay) {
    heroLay = new QVBoxLayout(heroContent);
    heroLay->setContentsMargins(0, 0, 0, 0);
    heroLay->setSpacing(0);
  }

  auto* titleBlock = new QWidget(heroContent);
  titleBlock->setObjectName(QStringLiteral("LandingTitleBlock"));
  auto* titleLay = new QVBoxLayout(titleBlock);
  titleLay->setContentsMargins(0, 0, 0, 0);
  titleLay->setSpacing(0);
  auto addTitleLine = [&](const QString& text, const QString& objectName) {
    auto* line = new QLabel(text, titleBlock);
    line->setObjectName(objectName);
    line->setWordWrap(false);
    line->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    titleLay->addWidget(line);
  };
  addTitleLine(QStringLiteral("Подберём"), QStringLiteral("LandingH1Line"));
  addTitleLine(QStringLiteral("автомобиль"), QStringLiteral("LandingH1Line"));
  addTitleLine(QStringLiteral("под ваши задачи"), QStringLiteral("LandingH1Accent"));

  auto* lead = new QLabel(
      QStringLiteral("Поможем найти автомобиль по критериям и бюджету — быстро и без лишних хлопот."), heroContent);
  lead->setObjectName(QStringLiteral("LandingLead"));
  lead->setWordWrap(true);

  auto* props = new QHBoxLayout();
  props->setContentsMargins(0, 18, 0, 22);
  props->setSpacing(22);
  auto makeProp = [&](const QString& icon, const QString& lines) {
    auto* wrap = new QWidget(heroContent);
    wrap->setObjectName(QStringLiteral("LandingProp"));
    auto* hl = new QHBoxLayout(wrap);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(10);
    auto* ic = new QLabel(icon, wrap);
    ic->setObjectName(QStringLiteral("LandingPropIcon"));
    ic->setAlignment(Qt::AlignCenter);
    auto* tx = new QLabel(lines, wrap);
    tx->setObjectName(QStringLiteral("LandingPropText"));
    hl->addWidget(ic, 0, Qt::AlignVCenter);
    hl->addWidget(tx, 0, Qt::AlignVCenter);
    props->addWidget(wrap, 0, Qt::AlignLeft);
  };
  makeProp(QStringLiteral("🧭"), QStringLiteral("Тщательный подбор"));
  makeProp(QStringLiteral("🎛"), QStringLiteral("Индивидуальный подход"));

  auto* ctaRow = new QHBoxLayout();
  ctaRow->setContentsMargins(0, 0, 0, 0);
  ctaRow->setSpacing(18);
  auto* ctaPrimary = new LandingSolidCta(QStringLiteral("Подобрать автомобиль"), heroContent);
  ctaPrimary->onClicked = [this]() { goToStackPage(kWizard); };
  auto* ctaGhost = new LandingGhostCta(QStringLiteral("Как мы работаем"), heroContent);
  ctaGhost->onClicked = [this]() { scrollHomeTo(secHow_); };
  ctaRow->addWidget(ctaPrimary, 0, Qt::AlignLeft | Qt::AlignVCenter);
  ctaRow->addWidget(ctaGhost, 0, Qt::AlignLeft | Qt::AlignVCenter);
  ctaRow->addStretch(1);

  heroLay->addWidget(titleBlock);
  heroLay->addSpacing(14);
  heroLay->addWidget(lead);
  heroLay->addLayout(props);
  heroLay->addLayout(ctaRow);

  outer->addWidget(homeHero_);

  auto* sectionsWrap = new QWidget(inner);
  sectionsWrap->setAttribute(Qt::WA_TranslucentBackground, true);
  auto* sectionsLay = new QVBoxLayout(sectionsWrap);
  sectionsLay->setContentsMargins(28, 18, 28, 32);
  sectionsLay->setSpacing(18);

  secServices_ = makeLandingSection(sectionsWrap, QStringLiteral("Услуги"),
                                      QStringLiteral("Место под описание услуг (как на сайте; можно заполнить позже)."),
                                      QStringLiteral("secServices"));
  sectionsLay->addWidget(secServices_);
  secHow_ = makeLandingSection(sectionsWrap, QStringLiteral("Этапы работы"),
                                QStringLiteral("1) Уточняем требования → 2) задаём доп. вопросы → 3) выдаём топ вариантов."),
                                QStringLiteral("secHow"));
  sectionsLay->addWidget(secHow_);
  secReviews_ = makeLandingSection(sectionsWrap, QStringLiteral("Отзывы"), QStringLiteral("Место под отзывы."),
                                    QStringLiteral("secReviews"));
  sectionsLay->addWidget(secReviews_);
  secAbout_ = makeLandingSection(sectionsWrap, QStringLiteral("О компании"), QStringLiteral("Место под описание."),
                                  QStringLiteral("secAbout"));
  sectionsLay->addWidget(secAbout_);

  auto* devCard = makeCard(sectionsWrap);
  auto* cv = new QVBoxLayout(devCard);
  cv->setContentsMargins(18, 16, 18, 16);
  auto* hint = new QLabel(
      QStringLiteral("Все разделы работают в этом окне через API сервера <b>car_picker</b> (сессия по cookie, как в браузере). "
                     "Запустите сервер: <code>./car_picker</code> на порту 8080."),
      devCard);
  hint->setObjectName(QStringLiteral("FormHint"));
  hint->setWordWrap(true);
  hint->setTextFormat(Qt::RichText);
  cv->addWidget(hint);
  sectionsLay->addWidget(devCard);

  outer->addWidget(sectionsWrap);

  homeScroll_ = scroll;
  stack_->addWidget(scroll);
  updateHomeHeroHeight();
}

void MainWindow::setupChatTab() {
  auto* page = new QWidget(stack_);
  page->setObjectName(QStringLiteral("StackPage"));
  prepareStackPage(page);
  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(28, 20, 28, 12);
  v->setSpacing(10);
  v->setSizeConstraint(QLayout::SetNoConstraint);

  auto* t = new QLabel(QStringLiteral("Чат‑помощник"), page);
  t->setObjectName(QStringLiteral("PageTitle"));
  auto* sub = new QLabel(QStringLiteral("Опишите задачу — ответ придёт так же, как на сайте."), page);
  sub->setObjectName(QStringLiteral("PageLead"));
  sub->setWordWrap(true);
  v->addWidget(t);
  v->addWidget(sub);
  addClientToolsBar(v, page);

  auto* histCard = makeCard(page);
  histCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* hv = new QVBoxLayout(histCard);
  hv->setContentsMargins(8, 8, 8, 8);
  hv->setSpacing(0);
  chatHistory_ = new ChatHistoryView(histCard);
  chatHistory_->setMinimumHeight(120);
  hv->addWidget(chatHistory_, 1);
  v->addWidget(histCard, 1);

  auto* bar = new QFrame(page);
  bar->setObjectName(QStringLiteral("ChatBar"));
  bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  bar->setFixedHeight(56);
  auto* row = new QHBoxLayout(bar);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(0);
  chatInput_ = new QLineEdit(bar);
  chatInput_->setPlaceholderText(QStringLiteral("Например: кроссовер до 2 млн, семья 5 человек, автомат"));
  chatSendBtn_ = new QPushButton(QStringLiteral("Отправить"), bar);
  chatSendBtn_->setObjectName(QStringLiteral("ChatSend"));
  chatSendBtn_->setCursor(Qt::PointingHandCursor);
  row->addWidget(chatInput_, 1);
  row->addWidget(chatSendBtn_);
  v->addWidget(bar, 0);

  connect(chatSendBtn_, &QPushButton::clicked, this, &MainWindow::onChatSend);
  connect(chatInput_, &QLineEdit::returnPressed, this, &MainWindow::onChatSend);

  stack_->addWidget(page);
}

void MainWindow::setupWizardTab() {
  auto* page = new QWidget(stack_);
  page->setObjectName(QStringLiteral("StackPage"));
  prepareStackPage(page);
  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(28, 20, 28, 16);
  v->setSpacing(10);

  auto* t = new QLabel(QStringLiteral("Мастер‑подбор"), page);
  t->setObjectName(QStringLiteral("PageTitle"));
  auto* sub = new QLabel(QStringLiteral("Задайте фильтры — в таблице появится топ совпадений; ♥ синхронизируется с сайтом."), page);
  sub->setObjectName(QStringLiteral("PageLead"));
  sub->setWordWrap(true);
  v->addWidget(t);
  v->addWidget(sub);
  addClientToolsBar(v, page);

  wizardSplitter_ = new QSplitter(Qt::Vertical, page);
  wizardSplitter_->setChildrenCollapsible(true);
  wizardSplitter_->setHandleWidth(5);
  auto* splitter = wizardSplitter_;

  auto* upper = new QWidget(splitter);
  auto* uv = new QVBoxLayout(upper);
  uv->setContentsMargins(0, 0, 0, 0);
  uv->setSpacing(12);

  auto* formCard = makeCard(upper);
  auto* fv = new QVBoxLayout(formCard);
  fv->setContentsMargins(16, 14, 16, 14);
  auto* form = new QFormLayout();
  wizMaxPrice_ = new QLineEdit(formCard);
  wizMaxPrice_->setPlaceholderText(QStringLiteral("например 2000000"));
  wizMinSeats_ = new QLineEdit(formCard);
  wizMinSeats_->setPlaceholderText(QStringLiteral("например 5"));
  wizBodyType_ = new QLineEdit(formCard);
  wizBodyType_->setPlaceholderText(QStringLiteral("sedan / hatchback / crossover / wagon / suv / van / liftback"));
  wizGearbox_ = new QLineEdit(formCard);
  wizGearbox_->setPlaceholderText(QStringLiteral("at / mt / cvt / robot"));
  wizDrive_ = new QLineEdit(formCard);
  wizDrive_->setPlaceholderText(QStringLiteral("fwd / rwd / awd"));

  form->addRow(QStringLiteral("Макс. бюджет (руб)"), wizMaxPrice_);
  form->addRow(QStringLiteral("Мин. мест"), wizMinSeats_);
  form->addRow(QStringLiteral("Тип кузова"), wizBodyType_);
  form->addRow(QStringLiteral("КПП"), wizGearbox_);
  form->addRow(QStringLiteral("Привод"), wizDrive_);
  fv->addLayout(form);

  wizRunBtn_ = new QPushButton(QStringLiteral("Подобрать"), formCard);
  wizRunBtn_->setObjectName(QStringLiteral("PrimaryBtn"));
  wizRunBtn_->setCursor(Qt::PointingHandCursor);
  connect(wizRunBtn_, &QPushButton::clicked, this, &MainWindow::onWizardRun);
  fv->addWidget(wizRunBtn_, 0, Qt::AlignLeft);
  uv->addWidget(formCard);

  auto* tableCard = makeCard(upper);
  auto* tv = new QVBoxLayout(tableCard);
  tv->setContentsMargins(8, 8, 8, 8);
  tv->setSpacing(6);
  auto* tableTitle = new QLabel(QStringLiteral("Результаты подбора"), tableCard);
  tableTitle->setObjectName(QStringLiteral("SectionTitle"));
  tv->addWidget(tableTitle);
  recsTable_ = new QTableWidget(tableCard);
  recsTable_->setColumnCount(7);
  recsTable_->setHorizontalHeaderLabels({QStringLiteral("♥"), QStringLiteral("Марка"), QStringLiteral("Модель"),
                                         QStringLiteral("Цена"), QStringLiteral("Кузов"), QStringLiteral("КПП"),
                                         QStringLiteral("Привод")});
  recsTable_->setAlternatingRowColors(true);
  recsTable_->setShowGrid(false);
  recsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  recsTable_->verticalHeader()->setDefaultSectionSize(36);
  recsTable_->horizontalHeader()->setStretchLastSection(true);
  recsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  recsTable_->setMinimumHeight(160);
  recsTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  tv->addWidget(recsTable_, 1);
  uv->addWidget(tableCard, 1);

  wizardInsightCard_ = makeCard(upper);
  auto* iv = new QVBoxLayout(wizardInsightCard_);
  iv->setContentsMargins(10, 10, 10, 10);
  auto* ih = new QLabel(QStringLiteral("Анализ стоимости владения (топ‑3)"), wizardInsightCard_);
  ih->setObjectName(QStringLiteral("FormHint"));
  wizardInsights_ = new QTextBrowser(wizardInsightCard_);
  wizardInsights_->setOpenExternalLinks(false);
  wizardInsights_->setMinimumHeight(150);
  wizardInsights_->setMaximumHeight(260);
  wizardInsights_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
  wizardInsights_->setVisible(false);
  wizardInsightCard_->setVisible(false);
  iv->addWidget(ih);
  iv->addWidget(wizardInsights_, 1);
  uv->addWidget(wizardInsightCard_, 0);

  myCarGroup_ = new QGroupBox(QStringLiteral("Мой авто → кандидаты (по желанию)"), splitter);
  myCarGroup_->setCheckable(true);
  myCarGroup_->setChecked(false);
  myCarGroup_->setFlat(true);
  myCarGroup_->setMaximumHeight(200);
  myCarGroup_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  auto* mv = new QVBoxLayout(myCarGroup_);
  mv->setContentsMargins(10, 8, 10, 8);
  mv->setSpacing(6);

  auto* myGrid = new QGridLayout();
  myGrid->setHorizontalSpacing(10);
  myGrid->setVerticalSpacing(6);
  myTrunk_ = new QLineEdit(myCarGroup_);
  myTrunk_->setPlaceholderText(QStringLiteral("багажник, л"));
  myFuel_ = new QLineEdit(myCarGroup_);
  myFuel_->setPlaceholderText(QStringLiteral("л/100"));
  myPower_ = new QLineEdit(myCarGroup_);
  myPower_->setPlaceholderText(QStringLiteral("л.с."));
  myPrice_ = new QLineEdit(myCarGroup_);
  myPrice_->setPlaceholderText(QStringLiteral("цена ₽"));
  myGrid->addWidget(new QLabel(QStringLiteral("Багажник"), myCarGroup_), 0, 0);
  myGrid->addWidget(myTrunk_, 0, 1);
  myGrid->addWidget(new QLabel(QStringLiteral("Расход"), myCarGroup_), 0, 2);
  myGrid->addWidget(myFuel_, 0, 3);
  myGrid->addWidget(new QLabel(QStringLiteral("Мощн."), myCarGroup_), 1, 0);
  myGrid->addWidget(myPower_, 1, 1);
  myGrid->addWidget(new QLabel(QStringLiteral("Цена"), myCarGroup_), 1, 2);
  myGrid->addWidget(myPrice_, 1, 3);
  mv->addLayout(myGrid);

  auto* myBtnRow = new QHBoxLayout();
  myEv_ = new QCheckBox(QStringLiteral("Электро"), myCarGroup_);
  auto* mySave = new QPushButton(QStringLiteral("Сравнить"), myCarGroup_);
  mySave->setObjectName(QStringLiteral("PrimaryBtn"));
  mySave->setCursor(Qt::PointingHandCursor);
  auto* myClear = new QPushButton(QStringLiteral("Сброс"), myCarGroup_);
  myClear->setObjectName(QStringLiteral("OutlineBtn"));
  myClear->setCursor(Qt::PointingHandCursor);
  connect(mySave, &QPushButton::clicked, this, &MainWindow::onMyCarSave);
  connect(myClear, &QPushButton::clicked, this, &MainWindow::onMyCarClear);
  myBtnRow->addWidget(myEv_);
  myBtnRow->addWidget(mySave);
  myBtnRow->addWidget(myClear);
  myBtnRow->addStretch();
  mv->addLayout(myBtnRow);

  myCarHint_ = new QLabel(myCarGroup_);
  myCarHint_->setObjectName(QStringLiteral("FormHint"));
  myCarHint_->setWordWrap(true);
  mv->addWidget(myCarHint_);

  myCarTable_ = new QTableWidget(myCarGroup_);
  myCarTable_->setColumnCount(9);
  myCarTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Кандидат"), QStringLiteral("Багажник"), QStringLiteral("Δ"), QStringLiteral("Расход"),
       QStringLiteral("Δ"), QStringLiteral("TCO/год"), QStringLiteral("Δ"), QStringLiteral("«5 лет» ₽/мес"),
       QStringLiteral("Δ")});
  myCarTable_->setAlternatingRowColors(true);
  myCarTable_->setShowGrid(true);
  myCarTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  myCarTable_->setSelectionMode(QAbstractItemView::NoSelection);
  myCarTable_->verticalHeader()->setVisible(false);
  myCarTable_->verticalHeader()->setDefaultSectionSize(26);
  myCarTable_->horizontalHeader()->setStretchLastSection(false);
  myCarTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  myCarTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  myCarTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  myCarTable_->setMinimumHeight(0);
  myCarTable_->setMaximumHeight(88);
  myCarTable_->setVisible(false);
  mv->addWidget(myCarTable_, 0);

  splitter->addWidget(upper);
  splitter->addWidget(myCarGroup_);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setSizes({900, 28});

  connect(myCarGroup_, &QGroupBox::toggled, this, [this](bool expanded) {
    if (!wizardSplitter_) return;
    const int total = qMax(400, wizardSplitter_->height());
    if (expanded)
      wizardSplitter_->setSizes({total - 168, 168});
    else
      wizardSplitter_->setSizes({total - 28, 28});
  });

  v->addWidget(splitter, 1);
  stack_->addWidget(page);
}

void MainWindow::setupCompareTab() {
  auto* page = new QWidget(stack_);
  page->setObjectName(QStringLiteral("StackPage"));
  prepareStackPage(page);
  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(28, 20, 28, 16);
  v->setSpacing(10);
  auto* t = new QLabel(QStringLiteral("Сравнение"), page);
  t->setObjectName(QStringLiteral("PageTitle"));
  auto* sub = new QLabel(
      QStringLiteral("До трёх моделей: из последнего топа мастера/чата (как на сайте) или укажите id вручную в запросе к API."),
      page);
  sub->setObjectName(QStringLiteral("PageLead"));
  sub->setWordWrap(true);
  v->addWidget(t);
  v->addWidget(sub);
  auto* card = makeCard(page);
  auto* cv = new QVBoxLayout(card);
  cv->setContentsMargins(12, 12, 12, 12);
  compareView_ = new QTextBrowser(card);
  compareView_->setOpenExternalLinks(false);
  connect(compareView_, &QTextBrowser::anchorClicked, this, [](const QUrl& url) {
    if (url.isValid()) QDesktopServices::openUrl(url);
  });
  compareView_->setMinimumHeight(0);
  compareView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  cv->addWidget(compareView_, 1);
  v->addWidget(card, 1);
  compareStackIndex_ = stack_->addWidget(page);
}

void MainWindow::setupLabTab() {
  labScroll_ = new QScrollArea(stack_);
  labScroll_->setObjectName(QStringLiteral("MainScroll"));
  labScroll_->setWidgetResizable(true);
  labScroll_->setFrameShape(QFrame::NoFrame);
  labScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  labScroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto* page = new QWidget(labScroll_);
  page->setObjectName(QStringLiteral("StackPage"));
  prepareStackPage(page);
  labScroll_->setWidget(page);

  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(28, 20, 28, 16);
  v->setSpacing(12);
  auto* t = new QLabel(QStringLiteral("Лаборатория"), page);
  t->setObjectName(QStringLiteral("PageTitle"));
  auto* sub = new QLabel(QStringLiteral("Метрики «5 лет», снежный индекс, слепой тест — те же расчёты, что на сайте."), page);
  sub->setObjectName(QStringLiteral("PageLead"));
  sub->setWordWrap(true);
  v->addWidget(t);
  v->addWidget(sub);

  labSenseCard_ = makeCard(page);
  auto* sv = new QVBoxLayout(labSenseCard_);
  sv->setContentsMargins(10, 10, 10, 10);
  labSenseView_ = new QTextBrowser(labSenseCard_);
  labSenseView_->setOpenExternalLinks(false);
  labSenseView_->setMinimumHeight(0);
  labSenseView_->setMaximumHeight(110);
  labSenseView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  sv->addWidget(labSenseView_);
  labSenseCard_->setVisible(false);
  v->addWidget(labSenseCard_, 0);

  labInsightGroup_ = new QGroupBox(QStringLiteral("Жизнь ~5 лет и снежный индекс (топ‑3)"), page);
  labInsightGroup_->setCheckable(true);
  labInsightGroup_->setChecked(true);
  labInsightGroup_->setFlat(true);
  auto* ig = new QVBoxLayout(labInsightGroup_);
  ig->setContentsMargins(10, 8, 10, 10);
  labInsightView_ = new QTextBrowser(labInsightGroup_);
  labInsightView_->setOpenExternalLinks(false);
  labInsightView_->setMinimumHeight(140);
  labInsightView_->setMaximumHeight(240);
  labInsightView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  ig->addWidget(labInsightView_);
  labInsightGroup_->setVisible(false);
  v->addWidget(labInsightGroup_, 0);

  labBlindCard_ = makeCard(page);
  labBlindCard_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* lv = new QVBoxLayout(labBlindCard_);
  lv->setContentsMargins(12, 12, 12, 12);
  lv->setSpacing(10);

  auto* blindTitle = new QLabel(QStringLiteral("Слепой тест"), labBlindCard_);
  blindTitle->setObjectName(QStringLiteral("SectionTitle"));
  lv->addWidget(blindTitle);

  labBlindHint_ = new QLabel(labBlindCard_);
  labBlindHint_->setObjectName(QStringLiteral("PageLead"));
  labBlindHint_->setWordWrap(true);
  lv->addWidget(labBlindHint_);

  blindUnavailable_ = new QLabel(QStringLiteral("В каталоге меньше трёх автомобилей — мини-игра недоступна."), labBlindCard_);
  blindUnavailable_->setObjectName(QStringLiteral("FormHint"));
  blindUnavailable_->setWordWrap(true);
  blindUnavailable_->setVisible(false);
  lv->addWidget(blindUnavailable_);

  blindCardsRow_ = new QWidget(labBlindCard_);
  blindCardsRow_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  blindCardsRow_->setMinimumHeight(220);
  blindCardsRow_->setMaximumHeight(300);
  auto* blindHBox = new QHBoxLayout(blindCardsRow_);
  blindHBox->setContentsMargins(0, 0, 0, 0);
  blindHBox->setSpacing(12);
  blindHBox->setAlignment(Qt::AlignTop);
  for (int i = 0; i < 3; ++i) {
    auto* card = new QFrame(blindCardsRow_);
    card->setObjectName(QStringLiteral("BlindCard"));
    card->setMinimumWidth(200);
    card->setMaximumWidth(340);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    blindCardFrame_[i] = card;
    auto* cv = new QVBoxLayout(card);
    cv->setContentsMargins(14, 12, 14, 12);
    cv->setSpacing(8);

    auto* topRow = new QHBoxLayout();
    topRow->addStretch();
    blindCardMark_[i] = new QLabel(QString(QChar('A' + i)), card);
    blindCardMark_[i]->setObjectName(QStringLiteral("BlindMark"));
    topRow->addWidget(blindCardMark_[i], 0, Qt::AlignRight);
    cv->addLayout(topRow);

    blindCardReveal_[i] = new QLabel(card);
    blindCardReveal_[i]->setObjectName(QStringLiteral("BlindReveal"));
    blindCardReveal_[i]->setWordWrap(true);
    blindCardReveal_[i]->setVisible(false);
    cv->addWidget(blindCardReveal_[i]);

    blindCardStats_[i] = new QLabel(card);
    blindCardStats_[i]->setObjectName(QStringLiteral("BlindStats"));
    blindCardStats_[i]->setWordWrap(true);
    blindCardStats_[i]->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    cv->addWidget(blindCardStats_[i]);

    blindPickBtns_[i] = new QPushButton(QStringLiteral("Выбрать %1").arg(QChar('A' + i)), card);
    blindPickBtns_[i]->setObjectName(QStringLiteral("BlindPickBtn"));
    blindPickBtns_[i]->setCursor(Qt::PointingHandCursor);
    blindPickBtns_[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const int pick = i;
    connect(blindPickBtns_[i], &QPushButton::clicked, this, [this, pick]() { onBlindPick(pick); });
    cv->addWidget(blindPickBtns_[i]);

    blindHBox->addWidget(card, 1);
  }
  lv->addWidget(blindCardsRow_, 0);

  blindNewBtn_ = new QPushButton(QStringLiteral("Новая тройка"), labBlindCard_);
  blindNewBtn_->setObjectName(QStringLiteral("OutlineBtn"));
  blindNewBtn_->setCursor(Qt::PointingHandCursor);
  connect(blindNewBtn_, &QPushButton::clicked, this, &MainWindow::onBlindNewRound);
  lv->addWidget(blindNewBtn_, 0, Qt::AlignLeft);

  v->addWidget(labBlindCard_, 0);
  v->addStretch(0);

  labStackIndex_ = stack_->addWidget(labScroll_);
}

void MainWindow::setupAdminTab() {
  auto* page = new QWidget(stack_);
  page->setObjectName(QStringLiteral("StackPage"));
  prepareStackPage(page);
  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(28, 20, 28, 16);
  v->setSpacing(10);
  auto* t = new QLabel(QStringLiteral("База автомобилей"), page);
  t->setObjectName(QStringLiteral("PageTitle"));
  auto* sub = new QLabel(QStringLiteral("Список каталога SQLite — создание, изменение и удаление через те же правила, что в веб‑админке."), page);
  sub->setObjectName(QStringLiteral("PageLead"));
  sub->setWordWrap(true);
  v->addWidget(t);
  v->addWidget(sub);
  auto* row = new QHBoxLayout();
  adminRefBtn_ = new QPushButton(QStringLiteral("Обновить"), page);
  adminRefBtn_->setObjectName(QStringLiteral("OutlineBtn"));
  adminAddBtn_ = new QPushButton(QStringLiteral("Добавить…"), page);
  adminAddBtn_->setObjectName(QStringLiteral("PrimaryBtn"));
  adminDelBtn_ = new QPushButton(QStringLiteral("Удалить выбранное"), page);
  adminDelBtn_->setObjectName(QStringLiteral("DangerBtn"));
  connect(adminRefBtn_, &QPushButton::clicked, this, &MainWindow::onAdminRefresh);
  connect(adminAddBtn_, &QPushButton::clicked, this, &MainWindow::onAdminAdd);
  connect(adminDelBtn_, &QPushButton::clicked, this, &MainWindow::onAdminDelete);
  row->addWidget(adminRefBtn_);
  row->addWidget(adminAddBtn_);
  row->addWidget(adminDelBtn_);
  row->addStretch();
  v->addLayout(row);
  auto* tc = makeCard(page);
  auto* tv = new QVBoxLayout(tc);
  tv->setContentsMargins(6, 6, 6, 6);
  adminTable_ = new QTableWidget(tc);
  adminTable_->setColumnCount(8);
  adminTable_->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("Марка"), QStringLiteral("Модель"), QStringLiteral("Цена"),
                                            QStringLiteral("Кузов"), QStringLiteral("КПП"), QStringLiteral("Привод"), QStringLiteral("Мест")});
  adminTable_->setAlternatingRowColors(true);
  adminTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  adminTable_->horizontalHeader()->setStretchLastSection(true);
  tv->addWidget(adminTable_, 1);
  v->addWidget(tc, 1);
  adminStackIndex_ = stack_->addWidget(page);
}

void MainWindow::setupFavoritesTab() {
  auto* page = new QWidget(stack_);
  page->setObjectName(QStringLiteral("StackPage"));
  prepareStackPage(page);
  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(28, 20, 28, 16);
  v->setSpacing(10);

  auto* t = new QLabel(QStringLiteral("Избранное"), page);
  t->setObjectName(QStringLiteral("PageTitle"));
  auto* sub = new QLabel(QStringLiteral("Сравнение до трёх первых моделей в порядке добавления — как на сайте."), page);
  sub->setObjectName(QStringLiteral("PageLead"));
  sub->setWordWrap(true);
  v->addWidget(t);
  v->addWidget(sub);

  auto* row = new QHBoxLayout();
  favClearBtn_ = new QPushButton(QStringLiteral("Очистить избранное"), page);
  favClearBtn_->setObjectName(QStringLiteral("DangerBtn"));
  favClearBtn_->setCursor(Qt::PointingHandCursor);
  connect(favClearBtn_, &QPushButton::clicked, this, &MainWindow::onClearFavorites);
  row->addWidget(favClearBtn_);
  row->addStretch();
  v->addLayout(row);

  auto* tableCard = makeCard(page);
  auto* tv = new QVBoxLayout(tableCard);
  tv->setContentsMargins(8, 8, 8, 8);
  favTable_ = new QTableWidget(tableCard);
  favTable_->setColumnCount(6);
  favTable_->setHorizontalHeaderLabels(
      {QStringLiteral("Марка"), QStringLiteral("Модель"), QStringLiteral("Цена"), QStringLiteral("Кузов"),
       QStringLiteral("КПП"), QStringLiteral("Привод")});
  favTable_->setAlternatingRowColors(true);
  favTable_->setShowGrid(false);
  favTable_->horizontalHeader()->setStretchLastSection(true);
  favTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  tv->addWidget(favTable_, 1);
  v->addWidget(tableCard, 1);

  auto* cmpCard = makeCard(page);
  auto* cv = new QVBoxLayout(cmpCard);
  cv->setContentsMargins(14, 12, 14, 12);
  auto* cmpLbl = new QLabel(QStringLiteral("Сравнение"), cmpCard);
  cmpLbl->setObjectName(QStringLiteral("PageTitle"));
  cmpLbl->setStyleSheet(QStringLiteral("font-size: 18px;"));
  cv->addWidget(cmpLbl);
  favCompare_ = new QTextEdit(cmpCard);
  favCompare_->setReadOnly(true);
  favCompare_->setMinimumHeight(0);
  favCompare_->setMaximumHeight(140);
  favCompare_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  cv->addWidget(favCompare_);
  cmpCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  cmpCard->setMaximumHeight(200);
  v->addWidget(cmpCard, 0);

  favoritesStackIndex_ = stack_->addWidget(page);
}

void MainWindow::getJson(const QString& path) {
  QUrl url(baseUrl_ + path);
  QNetworkRequest req(url);
  QNetworkReply* reply = net_->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, path, reply]() { handleReply(path, reply); });
}

void MainWindow::postForm(const QString& path, const QByteArray& formBody) {
  QUrl url(baseUrl_ + path);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  QNetworkReply* reply = net_->post(req, formBody);
  connect(reply, &QNetworkReply::finished, this, [this, path, reply]() { handleReply(path, reply); });
}

void MainWindow::addClientToolsBar(QVBoxLayout* layout, QWidget* parent) {
  auto* box = new QGroupBox(QStringLiteral("Справочник и отчёт (не влияет на подбор)"), parent);
  box->setCheckable(true);
  box->setChecked(false);
  auto* row = new QHBoxLayout(box);
  auto* edu = new QPushButton(QStringLiteral("Справочник"), box);
  auto* rep = new QPushButton(QStringLiteral("Отчёт для клиента"), box);
  auto* prof = new QPushButton(QStringLiteral("Данные клиента"), box);
  auto* top3 = new QPushButton(QStringLiteral("Разбор топ‑3"), box);
  edu->setCursor(Qt::PointingHandCursor);
  rep->setCursor(Qt::PointingHandCursor);
  prof->setCursor(Qt::PointingHandCursor);
  row->addWidget(edu);
  row->addWidget(rep);
  row->addWidget(prof);
  row->addWidget(top3);
  row->addStretch(1);
  connect(edu, &QPushButton::clicked, this, &MainWindow::onShowEducation);
  connect(rep, &QPushButton::clicked, this, &MainWindow::onShowClientReport);
  connect(prof, &QPushButton::clicked, this, &MainWindow::onShowClientProfile);
  connect(top3, &QPushButton::clicked, this, &MainWindow::onShowTop3Insights);
  layout->addWidget(box);
}

void MainWindow::onShowEducation() {
  pendingUiAction_ = QStringLiteral("education");
  getJson(QStringLiteral("/api/education"));
}

void MainWindow::onShowClientReport() {
  pendingUiAction_ = QStringLiteral("report");
  getJson(QStringLiteral("/api/client-report"));
}

void MainWindow::onShowClientProfile() {
  pendingUiAction_ = QStringLiteral("profile");
  getJson(QStringLiteral("/api/client-profile"));
}

void MainWindow::onShowTop3Insights() {
  pendingUiAction_ = QStringLiteral("top3");
  getJson(QStringLiteral("/api/top3-insights"));
}

void MainWindow::applyTop3InsightsFromJson(const QJsonArray& items) {
  if (!wizardInsights_ || items.isEmpty()) {
    if (wizardInsights_) wizardInsights_->setVisible(false);
    if (wizardInsightCard_) wizardInsightCard_->setVisible(false);
    return;
  }
  if (wizardInsightCard_) wizardInsightCard_->setVisible(true);
  QString html = QStringLiteral("<div style='color:#eaefff;font-size:13px'>");
  for (const QJsonValue& v : items) {
    const QJsonObject o = v.toObject();
    html += QStringLiteral("<h3 style='color:#25D0B9;margin:14px 0 6px'>#%1 %2 <span style='color:#b7c2f0;font-weight:normal'>%3%</span></h3>")
                .arg(o.value(QStringLiteral("rank")).toInt())
                .arg(o.value(QStringLiteral("title")).toString().toHtmlEscaped())
                .arg(o.value(QStringLiteral("score_pct")).toInt());
    html += QStringLiteral("<p><b>Рекомендация:</b> ") + o.value(QStringLiteral("explain_ru")).toString().toHtmlEscaped() +
            QStringLiteral("</p>");
    auto bullet = [&](const char* label, const char* key, const char* color) {
      const QJsonArray a = o.value(QLatin1String(key)).toArray();
      if (a.isEmpty()) return;
      html += QStringLiteral("<p style='color:%1;font-weight:600;margin:6px 0 2px'>").arg(QLatin1String(color)) + label +
              QStringLiteral("</p><ul style='margin:0 0 8px;padding-left:18px;color:#b7c2f0'>");
      for (const QJsonValue& it : a)
        html += QStringLiteral("<li>") + it.toString().toHtmlEscaped() + QStringLiteral("</li>");
      html += QStringLiteral("</ul>");
    };
    bullet("Сильные стороны", "strengths", "#5dffb0");
    bullet("Пожелания", "preferences", "#b7c2f0");
    bullet("На заметку", "cautions", "#ffb86b");
    const QJsonObject tco = o.value(QStringLiteral("tco")).toObject();
    const QJsonObject life = o.value(QStringLiteral("life_5y")).toObject();
    html += QStringLiteral("<p><b>TCO:</b> ~%1 ₽/год · топливо/энергия %2 · налог+ОСАГО %3<br/>"
                           "<b>5 лет:</b> ~%4 ₽/мес · всего за 5 лет %5 ₽</p>")
                .arg(tco.value(QStringLiteral("total_year_rub")).toInt())
                .arg(tco.value(QStringLiteral("fuel_energy_year_rub")).toInt())
                .arg(tco.value(QStringLiteral("tax_insurance_year_rub")).toInt())
                .arg(life.value(QStringLiteral("monthly_equiv_rub")).toInt())
                .arg(life.value(QStringLiteral("total_5y_rub")).toInt());
  }
  html += QStringLiteral("</div>");
  wizardInsights_->setHtml(html);
  wizardInsights_->setVisible(true);
}

void MainWindow::onChatSend() {
  const QString msg = chatInput_->text().trimmed();
  if (msg.isEmpty()) return;
  chatInput_->clear();

  appendChatHtml(chatHistory_, QStringLiteral("<b style='color:#25D0B9'>Вы:</b> ") + msg.toHtmlEscaped());

  QUrlQuery q;
  q.addQueryItem(QStringLiteral("message"), msg);
  postForm(QStringLiteral("/api/chat"), q.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::onWizardRun() {
  QUrlQuery q;
  if (!wizMaxPrice_->text().trimmed().isEmpty()) q.addQueryItem(QStringLiteral("max_price"), wizMaxPrice_->text().trimmed());
  if (!wizMinSeats_->text().trimmed().isEmpty()) q.addQueryItem(QStringLiteral("min_seats"), wizMinSeats_->text().trimmed());
  if (!wizBodyType_->text().trimmed().isEmpty()) q.addQueryItem(QStringLiteral("body_type"), wizBodyType_->text().trimmed());
  if (!wizGearbox_->text().trimmed().isEmpty()) q.addQueryItem(QStringLiteral("gearbox"), wizGearbox_->text().trimmed());
  if (!wizDrive_->text().trimmed().isEmpty()) q.addQueryItem(QStringLiteral("drive"), wizDrive_->text().trimmed());

  postForm(QStringLiteral("/api/wizard"), q.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::onClearFavorites() { postForm(QStringLiteral("/api/favorites/clear"), QByteArray()); }

void MainWindow::onMyCarSave() {
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("my_trunk"), myTrunk_->text().trimmed());
  q.addQueryItem(QStringLiteral("my_fuel"), myFuel_->text().trimmed());
  q.addQueryItem(QStringLiteral("my_power"), myPower_->text().trimmed());
  if (!myPrice_->text().trimmed().isEmpty()) q.addQueryItem(QStringLiteral("my_price"), myPrice_->text().trimmed());
  if (myEv_->isChecked()) q.addQueryItem(QStringLiteral("my_ev"), QStringLiteral("1"));
  postForm(QStringLiteral("/api/my-car"), q.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::onMyCarClear() { postForm(QStringLiteral("/api/my-car/clear"), QByteArray()); }

void MainWindow::setFavoriteIdsFromJson(const QJsonArray& ids) {
  favoriteIds_.clear();
  for (const auto& v : ids) favoriteIds_.push_back(v.toVariant().toLongLong());
}

void MainWindow::applyFavoritesPayload(const QJsonObject& obj) {
  setFavoriteIdsFromJson(obj.value(QStringLiteral("favorite_ids")).toArray());

  const QJsonArray cars = obj.value(QStringLiteral("cars")).toArray();
  favTable_->setRowCount(cars.size());
  for (int i = 0; i < cars.size(); ++i) {
    const QJsonObject c = cars[i].toObject();
    favTable_->setItem(i, 0, new QTableWidgetItem(c.value(QStringLiteral("brand")).toString()));
    favTable_->setItem(i, 1, new QTableWidgetItem(c.value(QStringLiteral("name")).toString()));
    favTable_->setItem(i, 2, new QTableWidgetItem(optToText(c.value(QStringLiteral("price")))));
    favTable_->setItem(i, 3, new QTableWidgetItem(c.value(QStringLiteral("body_type")).toString()));
    favTable_->setItem(i, 4, new QTableWidgetItem(c.value(QStringLiteral("gearbox")).toString()));
    favTable_->setItem(i, 5, new QTableWidgetItem(c.value(QStringLiteral("drive")).toString()));
  }
  refreshFavoritesCompareText(cars);
  refreshWizardHeartColumn();
}

bool MainWindow::isFavorite(qint64 carId) const {
  for (qint64 id : favoriteIds_) {
    if (id == carId) return true;
  }
  return false;
}

void MainWindow::refreshWizardHeartColumn() {
  if (!recsTable_) return;
  for (int row = 0; row < recsTable_->rowCount(); ++row) {
    auto* brandItem = recsTable_->item(row, 1);
    if (!brandItem) continue;
    const QVariant idVar = brandItem->data(Qt::UserRole);
    if (!idVar.isValid()) continue;
    const qint64 id = idVar.toLongLong();

    auto* heart = new QPushButton(isFavorite(id) ? QStringLiteral("♥") : QStringLiteral("♡"));
    heart->setObjectName(QStringLiteral("HeartBtn"));
    heart->setCursor(Qt::PointingHandCursor);
    heart->setToolTip(isFavorite(id) ? QStringLiteral("Убрать из избранного") : QStringLiteral("В избранное"));
    connect(heart, &QPushButton::clicked, this, [this, id]() {
      QUrlQuery q;
      q.addQueryItem(QStringLiteral("id"), QString::number(id));
      postForm(QStringLiteral("/api/favorite/toggle"), q.query(QUrl::FullyEncoded).toUtf8());
    });
    recsTable_->setCellWidget(row, 0, heart);
  }
}

void MainWindow::fillRecommendationsTable(const QJsonArray& recs) {
  recsTable_->setRowCount(recs.size());
  for (int i = 0; i < recs.size(); ++i) {
    const QJsonObject c = recs[i].toObject();
    const qint64 id = c.value(QStringLiteral("id")).toVariant().toLongLong();
    const QString brand = c.value(QStringLiteral("brand")).toString();
    const QString name = c.value(QStringLiteral("name")).toString();
    const QString price = optToText(c.value(QStringLiteral("price")));
    const QString body = c.value(QStringLiteral("body_type")).toString();
    const QString gb = c.value(QStringLiteral("gearbox")).toString();
    const QString dr = c.value(QStringLiteral("drive")).toString();

    auto* brandItem = new QTableWidgetItem(brand);
    brandItem->setData(Qt::UserRole, id);
    recsTable_->setItem(i, 1, brandItem);
    recsTable_->setItem(i, 2, new QTableWidgetItem(name));
    recsTable_->setItem(i, 3, new QTableWidgetItem(price));
    recsTable_->setItem(i, 4, new QTableWidgetItem(body));
    recsTable_->setItem(i, 5, new QTableWidgetItem(gb));
    recsTable_->setItem(i, 6, new QTableWidgetItem(dr));
  }
  refreshWizardHeartColumn();
}

void MainWindow::refreshFavoritesCompareText(const QJsonArray& cars) {
  if (cars.size() < 2) {
    favCompare_->setPlainText(QStringLiteral("Добавьте минимум две модели в избранное (♥ в мастере), чтобы появилось текстовое сравнение."));
    return;
  }
  const int n = qMin(3, cars.size());
  QStringList lines;
  lines.append(QStringLiteral("Поля\t") + QStringList([&]() {
    QStringList h;
    for (int i = 0; i < n; ++i) h << cars[i].toObject().value(QStringLiteral("brand")).toString();
    return h;
  }()).join(QStringLiteral("\t")));

  auto row = [&](const QString& label, const QString& key) {
    QStringList cells;
    cells << label;
    for (int i = 0; i < n; ++i) cells << optToText(cars[i].toObject().value(key));
    lines << cells.join(QStringLiteral("\t"));
  };

  row(QStringLiteral("Модель"), QStringLiteral("name"));
  row(QStringLiteral("Цена"), QStringLiteral("price"));
  row(QStringLiteral("Мест"), QStringLiteral("seats"));
  row(QStringLiteral("Багажник, л"), QStringLiteral("trunk"));
  row(QStringLiteral("Мощность"), QStringLiteral("power"));
  row(QStringLiteral("Расход"), QStringLiteral("fuel"));
  row(QStringLiteral("Кузов"), QStringLiteral("body_type"));
  row(QStringLiteral("КПП"), QStringLiteral("gearbox"));
  row(QStringLiteral("Привод"), QStringLiteral("drive"));
  row(QStringLiteral("TCO/год (руб)"), QStringLiteral("tco_year_rub"));
  {
    QStringList cells;
    cells << QStringLiteral("Соответствие, %");
    for (int i = 0; i < n; ++i) {
      const double s = cars[i].toObject().value(QStringLiteral("score")).toDouble();
      cells << QString::number((int)qRound(s * 100.0));
    }
    lines << cells.join(QStringLiteral("\t"));
  }

  favCompare_->setPlainText(lines.join(QStringLiteral("\n")));
}

static QString blindStatsText(const QJsonObject& c) {
  const int price = static_cast<int>(c.value(QStringLiteral("price")).toDouble());
  const double fuel = c.value(QStringLiteral("fuel")).toDouble();
  const bool ev = c.value(QStringLiteral("is_electric")).toBool();
  QString fuelLine = QStringLiteral("Расход: %1 л/100км").arg(QString::number(fuel, 'f', 1));
  if (ev) fuelLine += QStringLiteral(" (электро)");
  return QStringLiteral("Цена: %1 ₽\n%2\nКузов: %3\nБагажник: %4 л\nМощность: %5 л.с.")
      .arg(QLocale::system().toString(static_cast<qlonglong>(price)), fuelLine,
           c.value(QStringLiteral("body_type")).toString(),
           QString::number(static_cast<int>(c.value(QStringLiteral("trunk")).toDouble())),
           QString::number(static_cast<int>(c.value(QStringLiteral("power")).toDouble())));
}

void MainWindow::applyBlindTestUi(const QJsonArray& blindCars, int blindPick) {
  const bool ok = blindCars.size() == 3;
  if (blindCardsRow_) blindCardsRow_->setVisible(ok);
  if (blindUnavailable_) blindUnavailable_->setVisible(!ok);
  if (blindNewBtn_) blindNewBtn_->setVisible(ok);
  if (!ok) return;

  const bool reveal = blindPick >= 0 && blindPick <= 2;
  for (int i = 0; i < 3; ++i) {
    const QJsonObject c = blindCars[i].toObject();
    const bool picked = reveal && i == blindPick;
    if (blindCardFrame_[i])
      blindCardFrame_[i]->setObjectName(picked ? QStringLiteral("BlindCardPicked") : QStringLiteral("BlindCard"));
    if (blindCardStats_[i]) blindCardStats_[i]->setText(blindStatsText(c));
    if (blindCardReveal_[i]) {
      if (reveal) {
        const QString brand = c.value(QStringLiteral("brand")).toString();
        const QString name = c.value(QStringLiteral("name")).toString();
        blindCardReveal_[i]->setText(QStringLiteral("%1 %2\n%3 · %4 · %5")
                                         .arg(brand, name, brand, c.value(QStringLiteral("body_type")).toString(),
                                              c.value(QStringLiteral("drive")).toString()));
        blindCardReveal_[i]->setVisible(true);
      } else {
        blindCardReveal_[i]->clear();
        blindCardReveal_[i]->setVisible(false);
      }
    }
    if (blindPickBtns_[i]) {
      blindPickBtns_[i]->setVisible(!reveal);
      blindPickBtns_[i]->setEnabled(!reveal);
    }
    if (blindCardFrame_[i]) blindCardFrame_[i]->style()->unpolish(blindCardFrame_[i]);
    if (blindCardFrame_[i]) blindCardFrame_[i]->style()->polish(blindCardFrame_[i]);
  }
}

static QTableWidgetItem* myCarCell(const QString& text, const QColor& color = QColor(234, 240, 255)) {
  auto* it = new QTableWidgetItem(text);
  it->setForeground(color);
  it->setTextAlignment(Qt::AlignCenter);
  return it;
}

static QTableWidgetItem* myCarDeltaCell(int delta, bool moreIsBetter) {
  QString sign;
  if (delta > 0) sign = QStringLiteral("+");
  QColor color(183, 194, 240);
  if (delta != 0) {
    const bool good = moreIsBetter ? delta > 0 : delta < 0;
    color = good ? QColor(93, 255, 176) : QColor(255, 138, 138);
  }
  return myCarCell(sign + QString::number(delta), color);
}

static QTableWidgetItem* myCarMoneyDeltaCell(int delta) {
  QString sign;
  if (delta > 0) sign = QStringLiteral("+");
  QColor color(183, 194, 240);
  if (delta < 0) color = QColor(93, 255, 176);
  else if (delta > 0) color = QColor(255, 138, 138);
  return myCarCell(sign + QLocale::system().toString(static_cast<qlonglong>(delta)), color);
}

void MainWindow::applyMyCarPayload(const QJsonObject& root) {
  if (!myCarHint_ && !myCarTable_) return;
  const QJsonObject mc = root.value(QStringLiteral("my_car")).toObject();
  const bool configured = mc.value(QStringLiteral("configured")).toBool();
  if (configured) {
    if (myTrunk_) myTrunk_->setText(QString::number(mc.value(QStringLiteral("trunk_l")).toInt()));
    if (myFuel_) myFuel_->setText(QString::number(mc.value(QStringLiteral("fuel_l100")).toDouble(), 'f', 1));
    if (myPower_) myPower_->setText(QString::number(mc.value(QStringLiteral("power_hp")).toInt()));
    if (myPrice_) {
      const int pr = mc.value(QStringLiteral("approx_price_rub")).toInt(0);
      myPrice_->setText(pr > 0 ? QString::number(pr) : QString());
    }
    if (myEv_) myEv_->setChecked(mc.value(QStringLiteral("is_electric")).toBool());
  } else {
    if (myTrunk_) myTrunk_->clear();
    if (myFuel_) myFuel_->clear();
    if (myPower_) myPower_->clear();
    if (myPrice_) myPrice_->clear();
    if (myEv_) myEv_->setChecked(false);
  }
  const bool hasTop = root.value(QStringLiteral("my_car_has_top")).toBool();
  const QJsonArray rows = root.value(QStringLiteral("my_car_rows")).toArray();

  if (!hasTop) {
    myCarHint_->setText(QStringLiteral("Сначала «Подобрать»."));
    myCarTable_->setVisible(false);
    myCarTable_->setRowCount(0);
    return;
  }
  if (!configured) {
    myCarHint_->setText(QStringLiteral("Заполните поля и «Сравнить»."));
    myCarTable_->setVisible(false);
    myCarTable_->setRowCount(0);
    return;
  }
  if (rows.isEmpty()) {
    myCarHint_->setText(QStringLiteral("Нет кандидатов в топе для сравнения."));
    myCarTable_->setVisible(false);
    myCarTable_->setRowCount(0);
    return;
  }

  myCarHint_->setText(QStringLiteral("Зелёный Δ — лучше для вас, красный — хуже; для расхода и TCO меньше — лучше."));
  const QJsonObject ref = root.value(QStringLiteral("my_car_ref")).toObject();
  const int rowCount = rows.size() + (ref.isEmpty() ? 0 : 1);
  myCarTable_->setRowCount(rowCount);
  myCarTable_->setVisible(true);

  int row = 0;
  for (const QJsonValue& rv : rows) {
    const QJsonObject r = rv.toObject();
    auto* titleItem = new QTableWidgetItem(r.value(QStringLiteral("title")).toString());
    titleItem->setForeground(QColor(234, 240, 255));
    titleItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    myCarTable_->setItem(row, 0, titleItem);
    myCarTable_->setItem(row, 1, myCarCell(QString::number(r.value(QStringLiteral("trunk")).toInt()) + QStringLiteral(" л")));
    myCarTable_->setItem(row, 2, myCarDeltaCell(r.value(QStringLiteral("trunk_delta")).toInt(), true));
    myCarTable_->setItem(row, 3, myCarCell(QString::number(r.value(QStringLiteral("fuel")).toDouble(), 'f', 1)));

    const QString fk = r.value(QStringLiteral("fuel_delta_kind")).toString();
    if (fk == QStringLiteral("neu") && r.value(QStringLiteral("fuel_delta")).toDouble() == 0.0 &&
        (r.value(QStringLiteral("fuel_is_electric")).toBool() || mc.value(QStringLiteral("is_electric")).toBool()))
      myCarTable_->setItem(row, 4, myCarCell(QStringLiteral("—")));
    else {
      const double fd = r.value(QStringLiteral("fuel_delta")).toDouble();
      QString sign;
      if (fd > 1e-6) sign = QStringLiteral("+");
      QColor color(183, 194, 240);
      if (fk == QStringLiteral("good")) color = QColor(93, 255, 176);
      else if (fk == QStringLiteral("bad")) color = QColor(255, 138, 138);
      myCarTable_->setItem(row, 4, myCarCell(sign + QString::number(fd, 'f', 1) + QStringLiteral(" л"), color));
    }

    myCarTable_->setItem(row, 5,
                         myCarCell(QLocale::system().toString(static_cast<qlonglong>(r.value(QStringLiteral("tco_year")).toDouble()))));
    myCarTable_->setItem(row, 6, myCarMoneyDeltaCell(r.value(QStringLiteral("tco_delta")).toInt()));
    myCarTable_->setItem(row, 7,
                         myCarCell(QLocale::system().toString(static_cast<qlonglong>(r.value(QStringLiteral("monthly_5y")).toDouble()))));
    myCarTable_->setItem(row, 8, myCarMoneyDeltaCell(r.value(QStringLiteral("monthly_delta")).toInt()));
    ++row;
  }

  if (!ref.isEmpty()) {
    auto* refTitle = new QTableWidgetItem(ref.value(QStringLiteral("name")).toString());
    QFont f = refTitle->font();
    f.setBold(true);
    refTitle->setFont(f);
    refTitle->setForeground(QColor(37, 208, 185));
    refTitle->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    myCarTable_->setItem(row, 0, refTitle);
    myCarTable_->setItem(row, 1, myCarCell(QString::number(ref.value(QStringLiteral("trunk")).toInt()) + QStringLiteral(" л"),
                                             QColor(37, 208, 185)));
    myCarTable_->setItem(row, 2, myCarCell(QStringLiteral("—"), QColor(37, 208, 185)));
    const QString fuelRef = ref.value(QStringLiteral("is_electric")).toBool()
                                ? QStringLiteral("0 (электро)")
                                : QString::number(ref.value(QStringLiteral("fuel")).toDouble(), 'f', 1);
    myCarTable_->setItem(row, 3, myCarCell(fuelRef, QColor(37, 208, 185)));
    myCarTable_->setItem(row, 4, myCarCell(QStringLiteral("—"), QColor(37, 208, 185)));
    myCarTable_->setItem(row, 5,
                         myCarCell(QLocale::system().toString(static_cast<qlonglong>(ref.value(QStringLiteral("tco_year")).toDouble())),
                                   QColor(37, 208, 185)));
    myCarTable_->setItem(row, 6, myCarCell(QStringLiteral("—"), QColor(37, 208, 185)));
    myCarTable_->setItem(row, 7,
                         myCarCell(QLocale::system().toString(static_cast<qlonglong>(ref.value(QStringLiteral("monthly_5y")).toDouble())),
                                   QColor(37, 208, 185)));
    myCarTable_->setItem(row, 8, myCarCell(QStringLiteral("—"), QColor(37, 208, 185)));
  }
  myCarTable_->resizeRowsToContents();
}

void MainWindow::applyComparePayload(const QJsonObject& root) {
  if (!compareView_) return;
  if (root.value(QStringLiteral("empty")).toBool()) {
    const bool fs = root.value(QStringLiteral("from_session_top")).toBool();
    compareView_->setHtml(fs ? QStringLiteral("<p style='color:#eaefff'>Сначала получите топ в <b>мастере</b> или <b>чате</b> — "
                                              "тогда сюда подставятся до трёх последних вариантов.</p>")
                           : QStringLiteral("<p style='color:#eaefff'>Нет автомобилей для сравнения (укажите id в запросе "
                                            "<code>?ids=</code> или получите топ).</p>"));
    return;
  }
  const QJsonArray cars = root.value(QStringLiteral("cars")).toArray();
  const int wi = static_cast<int>(root.value(QStringLiteral("winner_index")).toDouble());
  QString html = QStringLiteral("<div style='color:#eaefff;font-size:14px'>");
  if (root.value(QStringLiteral("from_session_top")).toBool()) {
    html += QStringLiteral("<p>Сравниваются последние варианты из топа мастера/чата.</p>");
  }
  html += QStringLiteral("<h3 style='color:#25D0B9;margin:12px 0 6px'>Почему лидер</h3><p>")
      + root.value(QStringLiteral("leader_explain")).toString().toHtmlEscaped().replace(QLatin1Char('\n'),
                                                                                          QStringLiteral("<br/>"))
      + QStringLiteral("</p>");
  html += QStringLiteral("<p style='opacity:.75;font-size:13px;margin:0 0 10px'>По ссылке в названии — поиск на "
                         "<b>Авито</b> по этой строке каталога (марка, модель, год, кузов, КПП, привод, цена).</p>");
  html += QStringLiteral("<h3 style='color:#25D0B9;margin:16px 0 8px'>Таблица</h3>"
                         "<table cellpadding='8' cellspacing='0' style='border-collapse:collapse;width:100%;"
                         "border:1px solid rgba(255,255,255,.12)'>");
  html += QStringLiteral("<tr style='background:rgba(255,255,255,.06)'><th></th>");
  for (int i = 0; i < cars.size(); ++i) {
    const QJsonObject c = cars[i].toObject();
    QString title = c.value(QStringLiteral("title")).toString();
    if (title.isEmpty()) {
      title = QStringLiteral("%1 %2").arg(c.value(QStringLiteral("brand")).toString(),
                                          c.value(QStringLiteral("name")).toString());
    }
    const QString avito = c.value(QStringLiteral("avito_url")).toString();
    QString nm;
    if (!avito.isEmpty()) {
      nm = QStringLiteral("<a href=\"%1\" style=\"color:#6ecfff;text-decoration:underline\" "
                          "title=\"Поиск на Авито по этой строке каталога\">%2<span aria-hidden=\"true\"> ↗</span></a>")
               .arg(avito.toHtmlEscaped(), title.toHtmlEscaped());
    } else {
      nm = title.toHtmlEscaped();
    }
    if (i == wi) nm += QStringLiteral(" <span style='color:#FFD54F'>★</span>");
    html += QStringLiteral("<th style='text-align:left;border-bottom:1px solid rgba(255,255,255,.12)'>%1</th>").arg(nm);
  }
  html += QStringLiteral("</tr>");

  auto addRow = [&](const QString& label, const std::function<QString(const QJsonObject&)>& cell) {
    QString r = QStringLiteral("<tr><td style='font-weight:600;border-bottom:1px solid rgba(255,255,255,.08)'>%1</td>")
                    .arg(label.toHtmlEscaped());
    for (int i = 0; i < cars.size(); ++i) {
      QString v = cell(cars[i].toObject()).toHtmlEscaped();
      if (i == wi) v = QStringLiteral("<span style='background:rgba(37,208,185,.12)'>%1</span>").arg(v);
      r += QStringLiteral("<td style='border-bottom:1px solid rgba(255,255,255,.08)'>%1</td>").arg(v);
    }
    r += QStringLiteral("</tr>");
    html += r;
  };

  addRow(QStringLiteral("Соответствие, %"), [&](const QJsonObject& c) {
    const double s = c.value(QStringLiteral("score")).toDouble();
    return QString::number(static_cast<int>(qRound(s * 100.0)));
  });
  addRow(QStringLiteral("Цена, ₽"), [&](const QJsonObject& c) {
    const int p = static_cast<int>(c.value(QStringLiteral("price")).toDouble());
    return QLocale::system().toString(static_cast<qlonglong>(p));
  });
  addRow(QStringLiteral("Расход л/100"), [&](const QJsonObject& c) {
    return QString::number(c.value(QStringLiteral("fuel")).toDouble(), 'f', 1);
  });
  addRow(QStringLiteral("TCO в год, ₽"), [&](const QJsonObject& c) {
    return QLocale::system().toString(static_cast<qlonglong>(c.value(QStringLiteral("tco_year_rub")).toDouble()));
  });
  addRow(QStringLiteral("Кузов"), [&](const QJsonObject& c) { return c.value(QStringLiteral("body_type")).toString(); });
  addRow(QStringLiteral("КПП"), [&](const QJsonObject& c) { return c.value(QStringLiteral("gearbox")).toString(); });
  addRow(QStringLiteral("Привод"), [&](const QJsonObject& c) { return c.value(QStringLiteral("drive")).toString(); });
  addRow(QStringLiteral("Места"), [&](const QJsonObject& c) {
    return QString::number(static_cast<int>(c.value(QStringLiteral("seats")).toDouble()));
  });
  addRow(QStringLiteral("Багажник, л"), [&](const QJsonObject& c) {
    return QString::number(static_cast<int>(c.value(QStringLiteral("trunk")).toDouble()));
  });
  addRow(QStringLiteral("Мощность, л.с."), [&](const QJsonObject& c) {
    return QString::number(static_cast<int>(c.value(QStringLiteral("power")).toDouble()));
  });
  addRow(QStringLiteral("Vmax, км/ч"), [&](const QJsonObject& c) {
    return QString::number(static_cast<int>(c.value(QStringLiteral("max_speed")).toDouble()));
  });
  addRow(QStringLiteral("Электро"), [&](const QJsonObject& c) {
    return c.value(QStringLiteral("is_electric")).toBool() ? QStringLiteral("да") : QStringLiteral("нет");
  });
  addRow(QStringLiteral("Сегмент"), [&](const QJsonObject& c) { return c.value(QStringLiteral("segment")).toString(); });

  html += QStringLiteral("<tr><td style='font-weight:600;vertical-align:top;border-bottom:0'>Комментарий</td>");
  for (int i = 0; i < cars.size(); ++i) {
    const QString ex = cars[i].toObject().value(QStringLiteral("explain_ru")).toString().toHtmlEscaped().replace(
        QLatin1Char('\n'), QStringLiteral("<br/>"));
    QString cell = QStringLiteral("<td style='vertical-align:top;border-bottom:0;font-size:13px'>%1</td>").arg(ex);
    if (i == wi)
      cell = QStringLiteral("<td style='vertical-align:top;border-bottom:0;font-size:13px;background:rgba(37,208,185,.1)'>%1</td>")
                 .arg(ex);
    html += cell;
  }
  html += QStringLiteral("</tr></table></div>");
  compareView_->setHtml(html);
}

void MainWindow::applyExtrasPayload(const QJsonObject& o) {
  const QString senseHtml = o.value(QStringLiteral("sensitivity_html")).toString().trimmed();
  const bool hasSense = !senseHtml.isEmpty() && senseHtml != QStringLiteral("<p></p>");
  if (labSenseView_) labSenseView_->setHtml(hasSense ? senseHtml : QString());
  if (labSenseCard_) labSenseCard_->setVisible(hasSense);

  const QJsonArray ins = o.value(QStringLiteral("insight")).toArray();
  if (ins.isEmpty()) {
    if (labInsightView_) labInsightView_->clear();
    if (labInsightGroup_) labInsightGroup_->setVisible(false);
  } else {
    QString insightHtml = QStringLiteral("<div style='color:#eaefff;font-size:14px;line-height:1.45'>");
    for (const auto& v : ins) {
      const QJsonObject it = v.toObject();
      const QJsonObject car = it.value(QStringLiteral("car")).toObject();
      const QJsonObject life = it.value(QStringLiteral("life_5y")).toObject();
      const QJsonObject snow = it.value(QStringLiteral("snow")).toObject();
      const QString title = QStringLiteral("%1 %2")
                                .arg(car.value(QStringLiteral("brand")).toString().toHtmlEscaped(),
                                     car.value(QStringLiteral("name")).toString().toHtmlEscaped());
      insightHtml += QStringLiteral("<h4 style='color:#25D0B9;margin:12px 0 6px'>%1</h4>").arg(title);
      insightHtml +=
          QStringLiteral("<p style='margin:0 0 6px'>%1</p>")
              .arg(car.value(QStringLiteral("explain_ru")).toString().toHtmlEscaped().replace(
                  QLatin1Char('\n'), QStringLiteral("<br/>")));
      const double monthly = life.value(QStringLiteral("monthly_equiv_rub")).toDouble();
      const double total5 = life.value(QStringLiteral("total_5y_rub")).toDouble();
      insightHtml += QStringLiteral("<p style='margin:4px 0'><b>5 лет с машиной:</b> эквивалент ~%1 ₽/мес, всего ~%2 ₽</p>")
                         .arg(QLocale::system().toString(static_cast<qlonglong>(monthly)),
                              QLocale::system().toString(static_cast<qlonglong>(total5)));
      insightHtml += QStringLiteral("<p style='margin:4px 0;color:#cfd6ee;font-size:13px'>%1</p>")
                        .arg(life.value(QStringLiteral("comment_ru")).toString().toHtmlEscaped());
      const double sn = snow.value(QStringLiteral("score_0_to_10")).toDouble();
      insightHtml += QStringLiteral("<p style='margin:6px 0 10px'><b>Снежный индекс:</b> %1/10 — %2. %3</p>")
                         .arg(QString::number(sn, 'f', 1), snow.value(QStringLiteral("tier_ru")).toString().toHtmlEscaped(),
                              snow.value(QStringLiteral("explain_ru")).toString().toHtmlEscaped());
    }
    insightHtml += QStringLiteral("</div>");
    if (labInsightView_) labInsightView_->setHtml(insightHtml);
    if (labInsightGroup_) {
      labInsightGroup_->setVisible(true);
      if (!labInsightGroup_->isChecked()) labInsightGroup_->setChecked(true);
    }
  }

  const QJsonArray blindCars = o.value(QStringLiteral("blind_cars")).toArray();
  const QJsonValue jPick = o.value(QStringLiteral("blind_pick"));
  const int blindPick = jPick.isUndefined() ? -1 : jPick.toInt(-1);
  QString hint;
  if (blindCars.size() == 3) {
    hint = QStringLiteral("Три анонимных профиля слева направо (A → B → C). Выберите по цифрам — затем откроются марки.");
    if (blindPick >= 0 && blindPick <= 2)
      hint += QStringLiteral(" Вы выбрали вариант %1.").arg(QChar('A' + blindPick));
  } else {
    hint = QStringLiteral("Слепой тест недоступен: в базе слишком мало записей.");
  }
  if (labBlindHint_) labBlindHint_->setText(hint);
  applyBlindTestUi(blindCars, blindPick);
}

void MainWindow::applyAdminCarsPayload(const QJsonObject& obj) {
  if (!adminTable_) return;
  const QJsonArray cars = obj.value(QStringLiteral("cars")).toArray();
  adminTable_->setRowCount(cars.size());
  for (int i = 0; i < cars.size(); ++i) {
    const QJsonObject c = cars[i].toObject();
    adminTable_->setItem(i, 0, new QTableWidgetItem(QString::number(c.value(QStringLiteral("id")).toVariant().toLongLong())));
    adminTable_->setItem(i, 1, new QTableWidgetItem(c.value(QStringLiteral("brand")).toString()));
    adminTable_->setItem(i, 2, new QTableWidgetItem(c.value(QStringLiteral("name")).toString()));
    adminTable_->setItem(i, 3, new QTableWidgetItem(QLocale::system().toString(
        static_cast<qlonglong>(static_cast<qint64>(c.value(QStringLiteral("price")).toDouble())))));
    adminTable_->setItem(i, 4, new QTableWidgetItem(c.value(QStringLiteral("body_type")).toString()));
    adminTable_->setItem(i, 5, new QTableWidgetItem(c.value(QStringLiteral("gearbox")).toString()));
    adminTable_->setItem(i, 6, new QTableWidgetItem(c.value(QStringLiteral("drive")).toString()));
    adminTable_->setItem(i, 7, new QTableWidgetItem(QString::number(static_cast<int>(c.value(QStringLiteral("seats")).toDouble()))));
  }
}

void MainWindow::onBlindPick(int pick) {
  if (pick < 0 || pick > 2) return;
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("pick"), QString::number(pick));
  postForm(QStringLiteral("/api/extras/blind/pick"), q.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::onBlindNewRound() { postForm(QStringLiteral("/api/extras/blind/new"), QByteArray()); }

void MainWindow::onAdminRefresh() { getJson(QStringLiteral("/api/admin/cars")); }

void MainWindow::onAdminDelete() {
  if (!adminTable_) return;
  const int row = adminTable_->currentRow();
  if (row < 0) {
    QMessageBox::information(this, QStringLiteral("База авто"), QStringLiteral("Выберите строку в таблице."));
    return;
  }
  auto* idIt = adminTable_->item(row, 0);
  if (!idIt) return;
  const qint64 id = idIt->text().toLongLong();
  if (id <= 0) return;
  if (QMessageBox::question(this, QStringLiteral("Удаление"),
                            QStringLiteral("Удалить автомобиль id=%1?").arg(id)) != QMessageBox::Yes)
    return;
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("id"), QString::number(id));
  postForm(QStringLiteral("/api/admin/cars/delete"), q.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::onAdminAdd() {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("Новый автомобиль"));
  auto* form = new QFormLayout(&dlg);
  auto* name = new QLineEdit(&dlg);
  auto* brand = new QLineEdit(&dlg);
  auto* price = new QLineEdit(QStringLiteral("2500000"), &dlg);
  auto* fuel = new QLineEdit(QStringLiteral("8.5"), &dlg);
  auto* body = new QLineEdit(QStringLiteral("седан"), &dlg);
  auto* gearbox = new QLineEdit(QStringLiteral("автомат"), &dlg);
  auto* drive = new QLineEdit(QStringLiteral("передний"), &dlg);
  auto* seats = new QLineEdit(QStringLiteral("5"), &dlg);
  auto* trunk = new QLineEdit(QStringLiteral("480"), &dlg);
  auto* power = new QLineEdit(QStringLiteral("150"), &dlg);
  auto* maxSpeed = new QLineEdit(QStringLiteral("210"), &dlg);
  auto* segment = new QComboBox(&dlg);
  segment->addItems({QStringLiteral("economy"), QStringLiteral("mass"), QStringLiteral("premium")});
  segment->setCurrentIndex(1);
  auto* elec = new QCheckBox(QStringLiteral("Электромобиль"), &dlg);
  form->addRow(QStringLiteral("Название *"), name);
  form->addRow(QStringLiteral("Марка"), brand);
  form->addRow(QStringLiteral("Цена, ₽ *"), price);
  form->addRow(QStringLiteral("Расход л/100"), fuel);
  form->addRow(QStringLiteral("Кузов *"), body);
  form->addRow(QStringLiteral("КПП *"), gearbox);
  form->addRow(QStringLiteral("Привод *"), drive);
  form->addRow(QStringLiteral("Места *"), seats);
  form->addRow(QStringLiteral("Багажник, л *"), trunk);
  form->addRow(QStringLiteral("Мощность, л.с. *"), power);
  form->addRow(QStringLiteral("Vmax, км/ч *"), maxSpeed);
  form->addRow(QStringLiteral("Сегмент"), segment);
  form->addRow(QString(), elec);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  QUrlQuery q;
  q.addQueryItem(QStringLiteral("name"), name->text().trimmed());
  q.addQueryItem(QStringLiteral("brand"), brand->text().trimmed());
  q.addQueryItem(QStringLiteral("price"), price->text().trimmed());
  q.addQueryItem(QStringLiteral("fuel_consumption"), fuel->text().trimmed());
  q.addQueryItem(QStringLiteral("body_type"), body->text().trimmed());
  q.addQueryItem(QStringLiteral("gearbox"), gearbox->text().trimmed());
  q.addQueryItem(QStringLiteral("drive"), drive->text().trimmed());
  q.addQueryItem(QStringLiteral("seats"), seats->text().trimmed());
  q.addQueryItem(QStringLiteral("trunk"), trunk->text().trimmed());
  q.addQueryItem(QStringLiteral("power"), power->text().trimmed());
  q.addQueryItem(QStringLiteral("max_speed"), maxSpeed->text().trimmed());
  q.addQueryItem(QStringLiteral("segment"), segment->currentText());
  if (elec->isChecked()) q.addQueryItem(QStringLiteral("is_electric"), QStringLiteral("on"));
  postForm(QStringLiteral("/api/admin/cars/create"), q.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::handleReply(const QString& path, QNetworkReply* reply) {
  const QByteArray raw = reply->readAll();
  const auto err = reply->error();
  const QString errStr = reply->errorString();
  reply->deleteLater();

  if (err != QNetworkReply::NoError) {
    if (path == QStringLiteral("/api/chat")) {
      appendChatHtml(chatHistory_, QStringLiteral("<span style='color:#FF5E5E'><b>Ошибка:</b> ") + errStr.toHtmlEscaped() +
                     QStringLiteral("</span>"));
    } else if (path == QStringLiteral("/api/compare") && compareView_) {
      compareView_->setHtml(QStringLiteral("<p style='color:#FF5E5E'><b>Сеть:</b> ") + errStr.toHtmlEscaped() +
                            QStringLiteral("</p>"));
    } else if (path == QStringLiteral("/api/extras")) {
      if (labSenseView_) labSenseView_->setPlainText(errStr);
      if (labInsightView_) labInsightView_->setPlainText({});
    } else if (path == QStringLiteral("/api/admin/cars")) {
      QMessageBox::warning(this, QStringLiteral("База авто"), errStr);
    }
    return;
  }

  QJsonParseError pe{};
  const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
  if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
    if (path == QStringLiteral("/api/chat")) {
      appendChatHtml(chatHistory_, QStringLiteral("<span style='color:#FF5E5E'><b>Ошибка JSON:</b> ") +
                     pe.errorString().toHtmlEscaped() + QStringLiteral("</span>"));
    } else if (path.startsWith(QStringLiteral("/api/admin/"))) {
      QMessageBox::warning(this, QStringLiteral("База авто"),
                           QStringLiteral("Ответ сервера не JSON: %1").arg(pe.errorString()));
    }
    return;
  }

  const QJsonObject obj = doc.object();

  if (path == QStringLiteral("/api/chat")) {
    const QString answer = obj.value(QStringLiteral("answer")).toString();
    appendChatHtml(chatHistory_, QStringLiteral("<b style='color:#FF4D8D'>Помощник:</b><br/>") +
                   answer.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>")));
    if (obj.contains(QStringLiteral("favorite_ids"))) {
      setFavoriteIdsFromJson(obj.value(QStringLiteral("favorite_ids")).toArray());
      refreshWizardHeartColumn();
    }
    if (obj.contains(QStringLiteral("top3_insights")))
      applyTop3InsightsFromJson(obj.value(QStringLiteral("top3_insights")).toArray());
    if (obj.contains(QStringLiteral("my_car"))) applyMyCarPayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/top3-insights") && pendingUiAction_ == QStringLiteral("top3")) {
    pendingUiAction_.clear();
    applyTop3InsightsFromJson(obj.value(QStringLiteral("top3_insights")).toArray());
    if (stack_) stack_->setCurrentIndex(kWizard);
    return;
  }

  if (path == QStringLiteral("/api/wizard")) {
    const QJsonArray recs = obj.value(QStringLiteral("recs")).toArray();
    if (obj.contains(QStringLiteral("favorite_ids"))) setFavoriteIdsFromJson(obj.value(QStringLiteral("favorite_ids")).toArray());
    fillRecommendationsTable(recs);
    if (obj.contains(QStringLiteral("top3_insights")))
      applyTop3InsightsFromJson(obj.value(QStringLiteral("top3_insights")).toArray());
    if (obj.contains(QStringLiteral("my_car"))) applyMyCarPayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/my-car") || path == QStringLiteral("/api/my-car/clear")) {
    if (path == QStringLiteral("/api/my-car") && obj.contains(QStringLiteral("ok")) && !obj.value(QStringLiteral("ok")).toBool()) {
      const QString er = obj.value(QStringLiteral("error")).toString();
      if (myCarHint_) myCarHint_->setText(er);
      if (myCarTable_) {
        myCarTable_->setVisible(false);
        myCarTable_->setRowCount(0);
      }
      return;
    }
    applyMyCarPayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/favorites") || path == QStringLiteral("/api/favorite/toggle") ||
      path == QStringLiteral("/api/favorites/clear")) {
    applyFavoritesPayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/compare")) {
    applyComparePayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/extras")) {
    applyExtrasPayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/extras/blind/pick") || path == QStringLiteral("/api/extras/blind/new")) {
    getJson(QStringLiteral("/api/extras"));
    return;
  }

  if (path == QStringLiteral("/api/admin/cars")) {
    applyAdminCarsPayload(obj);
    return;
  }

  if (path == QStringLiteral("/api/admin/cars/create") || path == QStringLiteral("/api/admin/cars/update") ||
      path == QStringLiteral("/api/admin/cars/delete")) {
    if (obj.value(QStringLiteral("ok")).toBool())
      getJson(QStringLiteral("/api/admin/cars"));
    else {
      const QString er = obj.value(QStringLiteral("error")).toString();
      QMessageBox::warning(this, QStringLiteral("База авто"), er.isEmpty() ? QStringLiteral("Ошибка запроса.") : er);
    }
    return;
  }

  if (path == QStringLiteral("/api/education") && pendingUiAction_ == QStringLiteral("education")) {
    pendingUiAction_.clear();
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Справочник — режим обучения"));
    dlg.resize(640, 520);
    auto* lay = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    QString html = QStringLiteral("<p>Краткие пояснения для клиента, который плохо ориентируется в технике.</p>");
    const QJsonArray topics = obj.value(QStringLiteral("topics")).toArray();
    for (const QJsonValue& tv : topics) {
      const QJsonObject t = tv.toObject();
      html += QStringLiteral("<h3>%1</h3>%2").arg(t.value(QStringLiteral("title")).toString().toHtmlEscaped(),
                                                  t.value(QStringLiteral("body_html")).toString());
    }
    browser->setHtml(html);
    lay->addWidget(browser);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(buttons);
    dlg.exec();
    return;
  }

  if (path == QStringLiteral("/api/client-report") && pendingUiAction_ == QStringLiteral("report")) {
    pendingUiAction_.clear();
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Отчёт для клиента"));
    dlg.resize(720, 560);
    auto* lay = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setPlainText(obj.value(QStringLiteral("markdown")).toString());
    browser->setFontFamily(QStringLiteral("Menlo"));
    lay->addWidget(browser);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(buttons);
    dlg.exec();
    return;
  }

  if (path == QStringLiteral("/api/client-profile") && pendingUiAction_ == QStringLiteral("profile")) {
    pendingUiAction_.clear();
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Данные клиента"));
    auto* form = new QFormLayout(&dlg);
    auto* name = new QLineEdit(obj.value(QStringLiteral("name")).toString(), &dlg);
    auto* phone = new QLineEdit(obj.value(QStringLiteral("phone")).toString(), &dlg);
    auto* email = new QLineEdit(obj.value(QStringLiteral("email")).toString(), &dlg);
    auto* notes = new QTextEdit(&dlg);
    notes->setPlainText(obj.value(QStringLiteral("notes")).toString());
    notes->setMaximumHeight(72);
    auto* manager = new QTextEdit(&dlg);
    manager->setPlainText(obj.value(QStringLiteral("manager_notes")).toString());
    manager->setMaximumHeight(100);
    auto* edu = new QCheckBox(QStringLiteral("Режим обучения в чате"), &dlg);
    edu->setChecked(obj.value(QStringLiteral("education_mode")).toBool());
    form->addRow(QStringLiteral("Имя"), name);
    form->addRow(QStringLiteral("Телефон"), phone);
    form->addRow(QStringLiteral("E-mail"), email);
    form->addRow(QStringLiteral("Заметки"), notes);
    form->addRow(QStringLiteral("Рекомендации менеджера"), manager);
    form->addRow(QString(), edu);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_name"), name->text().trimmed());
    q.addQueryItem(QStringLiteral("client_phone"), phone->text().trimmed());
    q.addQueryItem(QStringLiteral("client_email"), email->text().trimmed());
    q.addQueryItem(QStringLiteral("client_notes"), notes->toPlainText().trimmed());
    q.addQueryItem(QStringLiteral("manager_notes"), manager->toPlainText().trimmed());
    if (edu->isChecked()) q.addQueryItem(QStringLiteral("education_mode"), QStringLiteral("1"));
    pendingUiAction_ = QStringLiteral("profile_save");
    postForm(QStringLiteral("/api/client-profile"), q.query(QUrl::FullyEncoded).toUtf8());
    return;
  }

  if (path == QStringLiteral("/api/client-profile") && pendingUiAction_ == QStringLiteral("profile_save")) {
    pendingUiAction_.clear();
    if (obj.value(QStringLiteral("ok")).toBool())
      QMessageBox::information(this, QStringLiteral("Данные клиента"), QStringLiteral("Сохранено."));
    return;
  }
}
