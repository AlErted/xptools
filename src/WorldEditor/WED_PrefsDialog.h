#ifndef WED_PREFSDIALOG_H
#define WED_PREFSDIALOG_H

struct	WED_Prefs {
	int		hydro_correct;
	int		hydro_simplify;
};

extern WED_Prefs	gWedPrefs;	

void	WED_ShowPrefsDialog(void);
void	WED_LoadPrefs(void);
void	WED_SavePrefs(void);

#endif
