public final class SystemServer{


  private void startOtherServices(){
    WindowManagerService wm = null;





     wm = WindowManagerService.main(context, inputManager,
                    mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL,
                    !mFirstBoot, mOnlyCore);


     ServiceManager.addService(Context.WINDOW_SERVICE, wm);
  }
}