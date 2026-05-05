// Приведенный ниже блок ifdef — это стандартный метод создания макросов, упрощающий процедуру
// экспорта из библиотек DLL. Все файлы данной DLL скомпилированы с использованием символа OXRANALOUATABACLIENT_EXPORTS
// Символ, определенный в командной строке. Этот символ не должен быть определен в каком-либо проекте,
// использующем данную DLL. Благодаря этому любой другой проект, исходные файлы которого включают данный файл, видит
// функции OXRANALOUATABACLIENT_API как импортированные из DLL, тогда как данная DLL видит символы,
// определяемые данным макросом, как экспортированные.
#ifdef OXRANALOUATABACLIENT_EXPORTS
#define OXRANALOUATABACLIENT_API __declspec(dllexport)
#else
#define OXRANALOUATABACLIENT_API __declspec(dllimport)
#endif

// Этот класс экспортирован из библиотеки DLL
class OXRANALOUATABACLIENT_API COXRANALOUATABACLIENT {
public:
	COXRANALOUATABACLIENT(void);
	// TODO: добавьте сюда свои методы.
};

extern OXRANALOUATABACLIENT_API int nOXRANALOUATABACLIENT;

OXRANALOUATABACLIENT_API int fnOXRANALOUATABACLIENT(void);
