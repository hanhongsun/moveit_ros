/*
 * Copyright (c) 2013, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Mario Prats
 */

#include <main_window.h>
#include <ui_utils.h>
#include <QShortcut>
#include <QFileDialog>

#include <assert.h>
#include <string>
#include <rviz/tool_manager.h>
#include <rviz/frame_manager.h>
#include <rviz/display_factory.h>
#include <rviz/selection/selection_manager.h>

namespace benchmark_tool
{

const char * MainWindow::ROBOT_DESCRIPTION_PARAM = "robot_description";
const char * MainWindow::ROBOT_DESCRIPTION_SEMANTIC_PARAM = "robot_description_semantic";
const unsigned int MainWindow::DEFAULT_WAREHOUSE_PORT = 33830;

MainWindow::MainWindow(int argc, char **argv, QWidget *parent) :
    QMainWindow(parent),
    goal_pose_dragging_(false)
{
  setWindowTitle("Benchmark Tool");

  ui_.setupUi(this);

  //Rviz render panel
  render_panel_ = new rviz::RenderPanel();
  ui_.render_widget->addWidget(render_panel_);
  ui_.splitter->setStretchFactor(1, 4);

  visualization_manager_ = new rviz::VisualizationManager(render_panel_);
  render_panel_->initialize(visualization_manager_->getSceneManager(), visualization_manager_);

  visualization_manager_->initialize();
  visualization_manager_->startUpdate();

  //Grid display
  visualization_manager_->createDisplay("rviz/Grid", "Grid", true);

  //Planning scene display
  scene_display_ = new moveit_rviz_plugin::PlanningSceneDisplay();
  scene_display_->setName("Planning Scene");
  scene_display_->subProp("Robot Description")->setValue(ROBOT_DESCRIPTION_PARAM);
  scene_display_->subProp("Scene Geometry")->subProp("Scene Alpha")->setValue(1.0);
  visualization_manager_->addDisplay( scene_display_, true );

  //Interactive Marker display
  int_marker_display_ = visualization_manager_->getDisplayFactory()->make("rviz/InteractiveMarkers");
  int_marker_display_->initialize(visualization_manager_);
  int_marker_display_->setEnabled(true);
  int_marker_display_->subProp("Update Topic")->setValue(QString::fromStdString(robot_interaction::RobotInteraction::INTERACTIVE_MARKER_TOPIC + "/update"));

  if ( scene_display_ )
  {
    configure();

    rviz::Tool * interact_tool = visualization_manager_->getToolManager()->addTool("rviz/Interact");
    if (interact_tool)
    {
      visualization_manager_->getToolManager()->setCurrentTool(interact_tool);
      interact_tool->activate();
    }
    visualization_manager_->getSelectionManager()->enableInteraction(true);

    //Setup ui: colors and icons
    ui_.db_connect_button->setStyleSheet("QPushButton { color : red }");

    ui_.goal_poses_open_button->setIcon(QIcon::fromTheme("document-open", QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon)));
    ui_.goal_poses_add_button->setIcon(QIcon::fromTheme("list-add", QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder)));
    ui_.goal_poses_remove_button->setIcon(QIcon::fromTheme("list-remove", QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton)));
    ui_.goal_poses_save_button->setIcon(QIcon::fromTheme("document-save", QApplication::style()->standardIcon(QStyle::SP_DriveFDIcon)));
    ui_.goal_switch_visibility_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton));

    ui_.start_states_open_button->setIcon(QIcon::fromTheme("document-open", QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon)));
    ui_.start_states_add_button->setIcon(QIcon::fromTheme("list-add", QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder)));
    ui_.start_states_remove_button->setIcon(QIcon::fromTheme("list-remove", QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton)));
    ui_.start_states_save_button->setIcon(QIcon::fromTheme("document-save", QApplication::style()->standardIcon(QStyle::SP_DriveFDIcon)));

    //Connect signals and slots
    connect(ui_.actionExit, SIGNAL( triggered(bool) ), this, SLOT( exitActionTriggered(bool) ));
    connect(ui_.actionOpen, SIGNAL( triggered(bool) ), this, SLOT( openActionTriggered(bool) ));
    connect(ui_.planning_group_combo, SIGNAL( currentIndexChanged ( const QString & ) ), this, SLOT( planningGroupChanged( const QString & ) ));
    connect(ui_.db_connect_button, SIGNAL( clicked() ), this, SLOT( dbConnectButtonClicked() ));
    connect(ui_.load_scene_button, SIGNAL( clicked() ), this, SLOT( loadSceneButtonClicked() ));
    connect(ui_.planning_scene_list, SIGNAL( itemDoubleClicked (QListWidgetItem *) ), this, SLOT( loadSceneButtonClicked(QListWidgetItem *) ));

    //Goal poses
    connect( ui_.goal_poses_add_button, SIGNAL( clicked() ), this, SLOT( createGoalPoseButtonClicked() ));
    connect( ui_.goal_poses_remove_button, SIGNAL( clicked() ), this, SLOT( deleteGoalsOnDBButtonClicked() ));
    connect( ui_.load_poses_filter_text, SIGNAL( returnPressed() ), this, SLOT( loadGoalsFromDBButtonClicked() ));
    connect( ui_.goal_poses_open_button, SIGNAL( clicked() ), this, SLOT( loadGoalsFromDBButtonClicked() ));
    connect( ui_.goal_poses_save_button, SIGNAL( clicked() ), this, SLOT( saveGoalsOnDBButtonClicked() ));
    connect( ui_.goal_switch_visibility_button, SIGNAL( clicked() ), this, SLOT( switchGoalVisibilityButtonClicked() ));
    connect( ui_.goal_poses_list, SIGNAL( itemSelectionChanged() ), this, SLOT( goalPoseSelectionChanged() ));
    connect( ui_.goal_poses_list, SIGNAL( itemDoubleClicked(QListWidgetItem *) ), this, SLOT( goalPoseDoubleClicked(QListWidgetItem *) ));
    connect( ui_.show_x_checkbox, SIGNAL( stateChanged( int ) ), this, SLOT( visibleAxisChanged( int ) ));
    connect( ui_.show_y_checkbox, SIGNAL( stateChanged( int ) ), this, SLOT( visibleAxisChanged( int ) ));
    connect( ui_.show_z_checkbox, SIGNAL( stateChanged( int ) ), this, SLOT( visibleAxisChanged( int ) ));
    connect( ui_.check_goal_collisions_button, SIGNAL( clicked( ) ), this, SLOT( checkGoalsInCollision( ) ));
    connect( ui_.check_goal_reachability_button, SIGNAL( clicked( ) ), this, SLOT( checkGoalsReachable( ) ));
    connect( ui_.load_results_button, SIGNAL( clicked( ) ), this, SLOT( loadBenchmarkResults( ) ));

    connect( ui_.start_states_add_button, SIGNAL( clicked() ), this, SLOT( saveStartStateButtonClicked() ));
    connect( ui_.start_states_remove_button, SIGNAL( clicked() ), this, SLOT( deleteStatesOnDBButtonClicked() ));
    connect( ui_.load_states_filter_text, SIGNAL( returnPressed() ), this, SLOT( loadStatesFromDBButtonClicked() ));
    connect( ui_.start_states_open_button, SIGNAL( clicked() ), this, SLOT( loadStatesFromDBButtonClicked() ));
    connect( ui_.start_states_save_button, SIGNAL( clicked() ), this, SLOT( saveStatesOnDBButtonClicked() ));
    connect( ui_.start_states_list, SIGNAL( itemDoubleClicked(QListWidgetItem*) ), this, SLOT( startStateItemDoubleClicked(QListWidgetItem*) ));

    QShortcut *copy_goals_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_C), ui_.goal_poses_list);
    connect(copy_goals_shortcut, SIGNAL( activated() ), this, SLOT( copySelectedGoalPoses() ) );

    //Start a QTimer for handling main loop jobs
    main_loop_jobs_timer_.reset(new QTimer(this));
    connect(main_loop_jobs_timer_.get(), SIGNAL( timeout() ), this, SLOT( executeMainLoopJobs() ));
    main_loop_jobs_timer_->start(1000 / MAIN_LOOP_RATE);
  }
  else
  {
    ROS_ERROR("Cannot load robot. Is the robot_description parameter set?");
    exit(0);
  }
}

MainWindow::~MainWindow()
{
}

void MainWindow::exitActionTriggered(bool)
{
  QApplication::quit();
}

void MainWindow::openActionTriggered(bool)
{
  QString urdf_path = QFileDialog::getOpenFileName(this, tr("Select a robot description file"), tr(""), tr("URDF files (*.urdf)"));

  if (!urdf_path.isEmpty())
  {
    QString srdf_path = QFileDialog::getOpenFileName(this, tr("Select a semantic robot description file"), tr(""), tr("SRDF files (*.srdf)"));

    if (!srdf_path.isEmpty())
    {
      loadNewRobot(urdf_path.toStdString(), srdf_path.toStdString());
    }
  }
}

void MainWindow::loadNewRobot(const std::string &urdf_path, const std::string &srdf_path)
{
  //Load urdf
  setStatus(STATUS_WARN, QString::fromStdString("Loading urdf " + urdf_path));
  std::ifstream urdf_input_stream(urdf_path.c_str());
  std::stringstream urdf_sstr;
  urdf_sstr << urdf_input_stream.rdbuf();
  ros::param::set(ROBOT_DESCRIPTION_PARAM, urdf_sstr.str());

  //Load srdf
  setStatus(STATUS_WARN, QString::fromStdString("Loading srdf " + srdf_path));
  std::ifstream srdf_input_stream(srdf_path.c_str());
  std::stringstream srdf_sstr;
  srdf_sstr << srdf_input_stream.rdbuf();
  ros::param::set(ROBOT_DESCRIPTION_SEMANTIC_PARAM, srdf_sstr.str());

  setStatus(STATUS_WARN, QString("Resetting scene display... "));
  std::string old_scene_name;
  if (scene_display_->getPlanningSceneRO())
    old_scene_name = scene_display_->getPlanningSceneRO()->getName();
  scene_display_->reset();

  if (configure())
  {
    //Reload the scene geometry if one scene was already loaded
    setStatus(STATUS_WARN, QString("Reloading scene... "));
    QList<QListWidgetItem *> found_items = ui_.planning_scene_list->findItems(QString::fromStdString(old_scene_name), Qt::MatchExactly);
    if (found_items.size() > 0)
    {
      found_items[0]->setSelected(true);
      loadSceneButtonClicked();
    }

    //Update the kinematic state associated to the goals
    setStatus(STATUS_WARN, QString("Updating goals... "));
    for (GoalPoseMap::iterator it = goal_poses_.begin(); it != goal_poses_.end(); ++it)
    {
      it->second.setKinematicState(scene_display_->getPlanningSceneRO()->getCurrentState());
      if (robot_interaction_->getActiveEndEffectors().size() > 0)
        it->second.setEndEffector(robot_interaction_->getActiveEndEffectors()[0]);
    }
    setStatus(STATUS_WARN, QString(""));
  }
}

bool MainWindow::configure()
{
  if ( ! scene_display_->getPlanningSceneMonitor() || ! scene_display_->getPlanningSceneMonitor()->getKinematicModel())
  {
    ROS_ERROR("Cannot load robot");
    ui_.load_scene_button->setEnabled(false);
    ui_.load_results_button->setEnabled(false);
    ui_.check_goal_collisions_button->setEnabled(false);
    ui_.check_goal_reachability_button->setEnabled(false);
    ui_.db_connect_button->setEnabled(false);
    ui_.goal_poses_add_button->setEnabled(false);
    ui_.load_poses_filter_text->setEnabled(false);
    ui_.goal_poses_open_button->setEnabled(false);
    ui_.goal_poses_save_button->setEnabled(false);
    ui_.goal_switch_visibility_button->setEnabled(false);
    ui_.start_states_add_button->setEnabled(false);
    ui_.start_states_remove_button->setEnabled(false);
    ui_.start_states_open_button->setEnabled(false);
    ui_.start_states_save_button->setEnabled(false);
    return false;
  }

  ui_.load_scene_button->setEnabled(true);
  ui_.load_results_button->setEnabled(true);
  ui_.check_goal_collisions_button->setEnabled(true);
  ui_.check_goal_reachability_button->setEnabled(true);
  ui_.db_connect_button->setEnabled(true);
  ui_.goal_poses_add_button->setEnabled(true);
  ui_.load_poses_filter_text->setEnabled(true);
  ui_.goal_poses_open_button->setEnabled(true);
  ui_.goal_poses_save_button->setEnabled(true);
  ui_.goal_switch_visibility_button->setEnabled(true);
  ui_.start_states_add_button->setEnabled(true);
  ui_.start_states_remove_button->setEnabled(true);
  ui_.start_states_open_button->setEnabled(true);
  ui_.start_states_save_button->setEnabled(true);

  //Set the fixed frame to the model frame
  setStatus(STATUS_WARN, QString("Setting fixed frame... "));
  visualization_manager_->setFixedFrame(QString(scene_display_->getPlanningSceneMonitor()->getKinematicModel()->getModelFrame().c_str()));
  int_marker_display_->setFixedFrame(QString::fromStdString(scene_display_->getPlanningSceneMonitor()->getKinematicModel()->getModelFrame()));

  //Get the list of planning groups and fill in the combo box
  setStatus(STATUS_WARN, QString("Updating planning groups... "));
  std::vector<std::string> group_names = scene_display_->getPlanningSceneMonitor()->getKinematicModel()->getJointModelGroupNames();
  ui_.planning_group_combo->clear();
  for (std::size_t i = 0; i < group_names.size(); i++)
  {
    ui_.planning_group_combo->addItem(QString(group_names[i].c_str()));
  }

  //Configure robot-dependent ui elements
  ui_.load_states_filter_text->setText(QString::fromStdString(scene_display_->getPlanningSceneMonitor()->getKinematicModel()->getName() + ".*"));

  //robot interaction
  setStatus(STATUS_WARN, QString("Resetting robot interaction... "));
  robot_interaction_.reset(new robot_interaction::RobotInteraction(scene_display_->getPlanningSceneMonitor()->getKinematicModel()));
  if (group_names.size() > 0 && robot_interaction_)
  {
    robot_interaction_->decideActiveComponents(group_names[0]);
    if (robot_interaction_->getActiveEndEffectors().size() == 0)
      ROS_WARN_STREAM("No end-effectors defined for robot " << scene_display_->getPlanningSceneMonitor()->getKinematicModel()->getName());
  }
  setStatus(STATUS_WARN, QString(""));

  return true;
}

void MainWindow::planningGroupChanged(const QString &text)
{
  robot_interaction_->decideActiveComponents(text.toStdString());
}

void MainWindow::dbConnectButtonClicked()
{
  addBackgroundJob(boost::bind(&MainWindow::dbConnectButtonClickedBackgroundJob, this));
}

void MainWindow::dbConnectButtonClickedBackgroundJob()
{
  if (planning_scene_storage_)
  {
    planning_scene_storage_.reset();
    robot_state_storage_.reset();
    constraints_storage_.reset();
    ui_.planning_scene_list->clear();

    addMainLoopJob(boost::bind(&setButtonTextAndColor, ui_.db_connect_button, "Disconnected", "QPushButton { color : red }"));
  }
  else
  {
    int port = -1;
    QStringList host_port;

    host_port = ui_.db_combo->currentText().split(":");
    if (host_port.size() > 1)
    {
      try
      {
        port = boost::lexical_cast<int>(host_port[1].toStdString());
      }
      catch (...)
      {
      }
    }
    else
      port = DEFAULT_WAREHOUSE_PORT;

    if (port > 0 && ! host_port[0].isEmpty())
    {
      addMainLoopJob(boost::bind(&setButtonTextAndColor, ui_.db_connect_button, "Connecting...", "QPushButton { color : yellow }"));
      try
      {
        planning_scene_storage_.reset(new moveit_warehouse::PlanningSceneStorage(host_port[0].toStdString(),
                                                                                 port, 5.0));
        robot_state_storage_.reset(new moveit_warehouse::RobotStateStorage(host_port[0].toStdString(),
                                                                           port, 5.0));
        constraints_storage_.reset(new moveit_warehouse::ConstraintsStorage(host_port[0].toStdString(),
                                                                            port, 5.0));
        addMainLoopJob(boost::bind(&setButtonTextAndColor, ui_.db_connect_button, "Getting data...", "QPushButton { color : yellow }"));

        //Get all the scenes
        populatePlanningSceneList();

        addMainLoopJob(boost::bind(&setButtonTextAndColor, ui_.db_connect_button, "Connected", "QPushButton { color : green }"));
      }
      catch(std::runtime_error &ex)
      {
        ROS_ERROR("%s", ex.what());
        addMainLoopJob(boost::bind(&setButtonTextAndColor, ui_.db_connect_button, "Disconnected", "QPushButton { color : red }"));
        addMainLoopJob(boost::bind(&showCriticalMessage, this, "Error", ex.what()));
        return;
      }
    }
    else
    {
      ROS_ERROR("Warehouse server must be introduced as host:port (eg. server.domain.com:33830");
      addMainLoopJob(boost::bind(&setButtonTextAndColor, ui_.db_connect_button, "Disconnected", "QPushButton { color : red }"));
      addMainLoopJob(boost::bind(&showCriticalMessage, this, "Error", "Malformed url. Warehouse host must be introduced as host:port"));
    }
  }
}

void MainWindow::populatePlanningSceneList(void)
{
  ui_.planning_scene_list->setUpdatesEnabled(false);

  ui_.planning_scene_list->clear();
  ui_.planning_scene_list->setSortingEnabled(true);
  ui_.planning_scene_list->sortItems(Qt::AscendingOrder);

  std::vector<std::string> names;
  planning_scene_storage_->getPlanningSceneNames(names);
  for (std::size_t i = 0; i < names.size(); ++i)
  {
    ui_.planning_scene_list->addItem(names[i].c_str());
  }

  ui_.planning_scene_list->setUpdatesEnabled(true);
}

void MainWindow::loadSceneButtonClicked(QListWidgetItem *item)
{
  addBackgroundJob(boost::bind(&MainWindow::loadSceneButtonClickedBackgroundJob, this));
}

void MainWindow::loadSceneButtonClicked(void)
{
  addBackgroundJob(boost::bind(&MainWindow::loadSceneButtonClickedBackgroundJob, this));
}

void MainWindow::loadSceneButtonClickedBackgroundJob(void)
{
  if (planning_scene_storage_)
  {
    QList<QListWidgetItem *> sel = ui_.planning_scene_list->selectedItems();
    if ( ! sel.empty())
    {
      QListWidgetItem *s = sel.front();
      std::string scene = s->text().toStdString();

      setStatusFromBackground(STATUS_INFO, QString::fromStdString("Attempting to load scene" + scene + "..."));

      moveit_warehouse::PlanningSceneWithMetadata scene_m;
      bool got_ps = false;
      try
      {
        got_ps = planning_scene_storage_->getPlanningScene(scene_m, scene);
      }
      catch (std::runtime_error &ex)
      {
        ROS_ERROR("%s", ex.what());
      }

      if (got_ps)
      {
        ROS_DEBUG("Loaded scene '%s'", scene.c_str());
        setStatusFromBackground(STATUS_INFO, QString::fromStdString("Rendering Scene..."));

        //Update the planning scene
        planning_scene_monitor::LockedPlanningSceneRW ps = scene_display_->getPlanningSceneRW();
        if (ps)
        {
          ps->setPlanningSceneMsg(*scene_m.get());
          scene_display_->queueRenderSceneGeometry();
        }

        //Configure ui elements
        ui_.load_poses_filter_text->setText(QString::fromStdString(scene + ".*"));
        setStatusFromBackground(STATUS_INFO, QString::fromStdString(""));
      }
      else
      {
        ROS_WARN("Failed to load scene '%s'. Has the message format changed since the scene was saved?", scene.c_str());
      }
    }
  }
}

void MainWindow::addBackgroundJob(const boost::function<void(void)> &job)
{
  background_process_.addJob(job);
}

void MainWindow::addMainLoopJob(const boost::function<void(void)> &job)
{
  boost::mutex::scoped_lock slock(main_loop_jobs_lock_);
  main_loop_jobs_.push_back(job);
}

void MainWindow::updateGoalPoseMarkers(float wall_dt, float ros_dt)
{
  if (goal_pose_dragging_) {
    for (GoalPoseMap::iterator it = goal_poses_.begin(); it != goal_poses_.end() ; ++it)
      if (it->second.isVisible())
        it->second.imarker->update(wall_dt);
  }
}

void MainWindow::executeMainLoopJobs()
{
  int_marker_display_->update(1.0 / MAIN_LOOP_RATE, 1.0 / MAIN_LOOP_RATE);
  updateGoalPoseMarkers(1.0 / MAIN_LOOP_RATE, 1.0 / MAIN_LOOP_RATE);

  main_loop_jobs_lock_.lock();
  while (!main_loop_jobs_.empty())
  {
    boost::function<void(void)> fn = main_loop_jobs_.front();
    main_loop_jobs_.pop_front();
    main_loop_jobs_lock_.unlock();
    try
    {
      fn();
    }
    catch(std::runtime_error &ex)
    {
      ROS_ERROR("Exception caught executing main loop job: %s", ex.what());
    }
    catch(...)
    {
      ROS_ERROR("Exception caught executing main loop job");
    }
    main_loop_jobs_lock_.lock();
  }
  main_loop_jobs_lock_.unlock();
}

void MainWindow::selectItemJob(QListWidgetItem *item, bool flag)
{
  item->setSelected(flag);
}

void MainWindow::setItemSelectionInList(const std::string &item_name, bool selection, QListWidget *list)
{
  QList<QListWidgetItem*> found_items = list->findItems(QString(item_name.c_str()), Qt::MatchExactly);
  for (std::size_t i = 0 ; i < found_items.size(); ++i)
    found_items[i]->setSelected(selection);
}

}