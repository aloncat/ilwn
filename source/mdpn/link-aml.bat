@if exist ..\..\..\aml echo Please, remove existing 'aml' file/directory first & exit /b
@mklink /J ..\..\..\aml aml
