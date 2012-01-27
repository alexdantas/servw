#!/bin/sh
#
#	Script que inicia 5 janelas do terminal solicitando a pagina
#	mandada como primeiro argumento
#

i=0
while [ $i -lt $1 ]
do
	echo "Iniciando cliente $i..."
#	gnome-terminal --tab -e "/bin/bash -c 'wget alexdantas:27015/$2; exec /bin/bash -i'"
	gnome-terminal --tab -e "wget alexdantas:27015/$2"
	i=$(( $i + 1 ))
done
