#pragma once

#include <QMainWindow>
#include <QVector>

class QButtonGroup;
class QFrame;
class QGroupBox;
class QSplitter;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class QTableWidget;
class QTextBrowser;
class QTextEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonArray;
class QJsonObject;
class QLabel;
class LandingHeroWidget;
class MainColumnBackdrop;
class QWidget;
class QResizeEvent;
class QShowEvent;

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private:
  void setupUi();
  void applyAppStyle();
  void setupHomePage();
  void setupChatTab();
  void setupWizardTab();
  void setupCompareTab();
  void setupFavoritesTab();
  void setupLabTab();
  void setupAdminTab();
  void addClientToolsBar(QVBoxLayout* layout, QWidget* parent);
  void onNavPage(int id);
  void goToStackPage(int stackIndex);
  void fetchLandingBackground();
  void tryFetchLandingBg(int attempt);
  void fetchSidebarLogo();
  void refreshSidebarLogo();
  void updateHomeHeroHeight();
  void scrollHomeTo(QWidget* section);
  void reloadPageData(int stackIndex);

  void getJson(const QString& path);
  void postForm(const QString& path, const QByteArray& formBody);

  void onChatSend();
  void onWizardRun();
  void onClearFavorites();
  void onBlindPick(int pick);
  void onBlindNewRound();
  void onMyCarSave();
  void onMyCarClear();
  void onAdminRefresh();
  void onAdminDelete();
  void onAdminAdd();

  void onShowEducation();
  void onShowClientReport();
  void onShowClientProfile();
  void onShowTop3Insights();

  void applyTop3InsightsFromJson(const QJsonArray& items);

  void handleReply(const QString& path, QNetworkReply* reply);

  void setFavoriteIdsFromJson(const QJsonArray& ids);
  void applyFavoritesPayload(const QJsonObject& obj);
  void applyComparePayload(const QJsonObject& obj);
  void applyExtrasPayload(const QJsonObject& obj);
  void applyMyCarPayload(const QJsonObject& obj);
  void applyBlindTestUi(const QJsonArray& blindCars, int blindPick);
  void applyAdminCarsPayload(const QJsonObject& obj);
  bool isFavorite(qint64 carId) const;
  void refreshWizardHeartColumn();
  void fillRecommendationsTable(const QJsonArray& recs);
  void refreshFavoritesCompareText(const QJsonArray& cars);

  QTextEdit* chatHistory_ = nullptr;
  QLineEdit* chatInput_ = nullptr;
  QPushButton* chatSendBtn_ = nullptr;

  QLineEdit* wizMaxPrice_ = nullptr;
  QLineEdit* wizMinSeats_ = nullptr;
  QLineEdit* wizBodyType_ = nullptr;
  QLineEdit* wizGearbox_ = nullptr;
  QLineEdit* wizDrive_ = nullptr;
  QPushButton* wizRunBtn_ = nullptr;

  QTableWidget* recsTable_ = nullptr;
  QWidget* wizardInsightCard_ = nullptr;
  QTextBrowser* wizardInsights_ = nullptr;
  QGroupBox* myCarGroup_ = nullptr;
  QSplitter* wizardSplitter_ = nullptr;

  QTextBrowser* compareView_ = nullptr;

  QStackedWidget* stack_ = nullptr;
  QButtonGroup* navGroup_ = nullptr;
  int favoritesStackIndex_ = -1;
  int compareStackIndex_ = -1;
  int labStackIndex_ = -1;
  int adminStackIndex_ = -1;

  QScrollArea* homeScroll_ = nullptr;
  LandingHeroWidget* homeHero_ = nullptr;
  QFrame* secServices_ = nullptr;
  QFrame* secHow_ = nullptr;
  QFrame* secReviews_ = nullptr;
  QFrame* secAbout_ = nullptr;

  QTableWidget* favTable_ = nullptr;
  QTextEdit* favCompare_ = nullptr;
  QPushButton* favClearBtn_ = nullptr;

  QWidget* labSenseCard_ = nullptr;
  QTextBrowser* labSenseView_ = nullptr;
  QGroupBox* labInsightGroup_ = nullptr;
  QTextBrowser* labInsightView_ = nullptr;
  QWidget* labBlindCard_ = nullptr;
  QScrollArea* labScroll_ = nullptr;
  QWidget* blindCardsRow_ = nullptr;
  QLabel* blindUnavailable_ = nullptr;
  QFrame* blindCardFrame_[3] = {nullptr, nullptr, nullptr};
  QLabel* blindCardMark_[3] = {nullptr, nullptr, nullptr};
  QLabel* blindCardReveal_[3] = {nullptr, nullptr, nullptr};
  QLabel* blindCardStats_[3] = {nullptr, nullptr, nullptr};
  QLabel* labBlindHint_ = nullptr;

  QLineEdit* myTrunk_ = nullptr;
  QLineEdit* myFuel_ = nullptr;
  QLineEdit* myPower_ = nullptr;
  QLineEdit* myPrice_ = nullptr;
  QCheckBox* myEv_ = nullptr;
  QLabel* myCarHint_ = nullptr;
  QTableWidget* myCarTable_ = nullptr;
  QPushButton* blindPickBtns_[3] = {nullptr, nullptr, nullptr};
  QPushButton* blindNewBtn_ = nullptr;

  QTableWidget* adminTable_ = nullptr;
  QPushButton* adminRefBtn_ = nullptr;
  QPushButton* adminAddBtn_ = nullptr;
  QPushButton* adminDelBtn_ = nullptr;

  QNetworkAccessManager* net_ = nullptr;

  MainColumnBackdrop* mainBackdrop_ = nullptr;
  QLabel* logoMark_ = nullptr;
  QLabel* mainFooter_ = nullptr;

  QString baseUrl_ = "http://127.0.0.1:8080";
  QVector<qint64> favoriteIds_;

  QString pendingUiAction_;
};
