Summary: Graphics file browser utility.
Summary(fr): Explorateur de fichiers graphiques
Summary(es): Navegador de archivos gr�ficos
Name: gqview
Version: 2.0.4
Release: 1
License: GPL
Group: Applications/Multimedia
Source: http://prdownloads.sourceforge.net/gqview/gqview-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

URL: http://gqview.sourceforge.net

Requires: gtk2 >= 2.4.0

%description
GQview is a browser for graphics files.
Offering single click viewing of your graphics files.
Includes thumbnail view, zoom and filtering features.
And external editor support.

%description -l fr
GQview est un explorateur de fichiers graphiques.
Il permet d'un simple clic l'affichage de vos fichiers graphiques.
Les capacit�s suivantes sont incluses: vue d'imagettes, zoom,
filtres et support d'�diteurs externes.

%description -l es
GQview es un navegador de archivos gr�ficos.
Ofrece visualizar sus archivos gr�ficos con s�lo hacer un clic.
Incluye visualizaci�n de miniaturas, zoom, filtros y soporte para
editores externos.

%prep
%setup

%build
if [ ! -f configure ]; then
  CFLAGS="$MYCFLAGS" ./autogen.sh $MYARCH_FLAGS --prefix=%{_prefix}
else
  CFLAGS="$MYCFLAGS" ./configure $MYARCH_FLAGS --prefix=%{_prefix}
fi

make

mkdir html
cp doc/*.html doc/*.txt html/.

%install
rm -rf $RPM_BUILD_ROOT

make mandir=$RPM_BUILD_ROOT%{_mandir} bindir=$RPM_BUILD_ROOT%{_bindir} \
 prefix=$RPM_BUILD_ROOT%{_prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)

%doc README COPYING TODO html
%{_bindir}/gqview
%{_datadir}/locale/*/*/*
%{_datadir}/applications/gqview.desktop
%{_datadir}/pixmaps/gqview.png
%{_mandir}/man?/*

