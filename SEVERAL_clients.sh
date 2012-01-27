#!/bin/sh
#
#	Script que inicia $1 janelas do terminal solicitando a pagina
#	mandada como segundo argumento
#

i=0
while [ $i -lt $1 ]
do
	echo "Iniciando cliente $i..."
	./several_clients.sh $2 $3
	i=$(( $i + 1 ))
done
