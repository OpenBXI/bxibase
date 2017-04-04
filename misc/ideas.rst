Ideas
==============

Project
----------

- Provide an official docker where bxibase is installed (openbxi/bxibase on dockerhub)
- Automate the production of such an official docker for each release (make docker)
- Provide a benchmark framework where bxilog is compared to other logging library (a VM
  in the cloud, AWS?)
- For each release candidate, the benchmark is launched and the release is produced according to it as
  with Pypy (http://speed.pypy.org/)
- Use Coverity for code quality check 

Interface
------------

- bxilog : bitmap de configuration (fonction, event, configuration, message,
  ... à la nagios). Pour les fonctions, proposer des macros: IN(level, args) 
  et OUT(level, return)
- last message : pour chaque niveau, retourne le dernier message reçu (bximonitor last)
- faire en sorte que le stacktrace soit disponible quelque soit le niveau de
  log (pas seulement TRACE).
- Fournit le nombre de logs envoyés par logger (et par thread afin d'éviter les locks?) 
    -> chaque thread peut compter son nombre de logs (par logger?). 
    Lorsque la demande est faite via l'interface, le nombre final est calculé : à ce 
    moment là on peut perdre du temps à agréger les résultats. Lorqu'une thread meurt,
    elle doit apporter sa contribution au nombre de logs générés pour chaque logger 
    utilisé... À voir, à étudier. ATTENTION: l'API aujourd'hui est lockless, ce serait
    bien qu'elle le reste. 


Implementation
----------------


- bxilog_handler -> en faire des plugins
- bxilog_handler -> pushing to logstash
- rendre la taille du buffer utilisé par chaque thread pour éviter le malloc 
  configurable (dynamique?)
- bxilog_file_handler -> Virer l'appel à localtime_r() qui est coûteux. bxilog-parser 
  sera capable de faire se travail et ce sera globalement moint couteux:  le parsing 
  sera plus rapide (un simple entier à lire au lieu de YY-MM-DDTHHMMSS.sss) et donc le 
  tri et le calcul de la fenêtre de tir sera plus rapide. La transformation en chaîne ne 
  sera faite qu'à la fin
- bxilog_file_handler ->  permettre l'affichage de la date/heure relativement au début 
  du programme ou pas à l'aide d'une option. 

- file_handler -> utiliser les aio pour asynchronisme (voir si c'est
  combinable avec les uio_vec ci-dessous).
- file_handler -> utiliser sys/uio.h pour écriture vectorisé. Lorsqu'un log
  record est reçu par le handler, il génère la string finale mais ne fait pas
  l'I/O. Il la stocke dans un uio_vec structure. Lorsqu'un nombre suffisamment
  important de telles structures ont été envoyés, on effectue l'appel système
- console_handler: virer les fputs()/fprintf() et tout appel qui passer par
  stdio.h. C'est coûteux et cela ne garantie pas l'atomicité des sorties.
- registry.h -> implémentation à base de tableau pas efficace. 
  Utiliser une structure de données adéquates type table de hash (par exemple 
  uthash.h mais non disponible dans POSIX) ou hsearch (de search.h dans POSIX mais non 
  réentrant, hsearch_r est réentrant mais GNU seulement) ou dans un arbre binaire 
  (tsearch qui est réentrant et POSIX).



bxilog-console/monitor
-----------------------------

- bximonitor: un handler de log capable de retourner une configuration clé/valeur et 
  qui permet de faire des get/set (à distance), par exemple -> changer la configuration 
  d'un ou des loggers

- Proposer une version à base de ncurses: les logs sont affichés à l'écran dans une 
  fenêtre qui couvre la majorité de l'écran. En menu en bas (à la htop) permet de 
  sélectionner le niveau de détails à la volée (la touche + augmente le niveau de détails,
  la touche - diminue le niveau de détail). Une autre fenêtre sur le côté présente 
  l'ensemble des loggers que l'on peut sélectionner directement avec les flèches et 
  'espace' par exemple. Une touche permet aussi de directement spécifier les filtres
  pour le handler courant (remote handler) ce qui ne touche pas aux filtres des autres 
  handlers. Enfin, la modification des filtres pour chaque handlers est proposé.
  La recherche doit être possible dans la fenêtre d'affichage des logs mais aussi dans 
  la fenêtre de présentation des loggers. 


bxilog-parser
-----------------


- bxilog-parser -> permettre le filtrage des logs parsés. Aujourd'hui tout est
  affiché. Il faudrait être capable de filtrer (avec le même format que la
  commande qui a produit les logs?).

- bxilogparser --from now -15mn: seek in the logging files in order to start 
  with the good date
- bxilog-parser : regroupe les logs similaires (même ligne, même fichier)
- bxilog-parser --last-error -> retourne la dernière erreur (dernier rapport
  d'erreur)
- bxilog-parser --follow (-F = --follow --retry) comme pour tail -F
- bxilog-parser fichier1.bxilog [...] fichierN.bxilog doit pouvoir fusionner les 
  fichiers de logs: attention il faut faire une fusion correcte en triant les entrées 
  correctement par timestamp. 


Misc
-------------

- Logger name with short versions (for console)
- cmd -v -> augmente le niveau de trace de 1 (pour la console uniquement?), cmd -vv idem, etc...
- cmd --quiet -> positionne le niveau de log console à off. 
- permettre le chargement d'une nouvelle configuration (reload) à la volée.
  Attention aux sides effects: par exemple, si on retire un remote handler et qu'une 
  connexion était active, que doit-il se passer? La connexion est rompue? On attend la 
  fin de la connexion du client?
- Permettre la rotation des fichiers de logs générés par le file_handler en fonction 
  d'une taille maximale ou d'une date.


