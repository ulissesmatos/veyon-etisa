# Mantendo este fork sincronizado com o Veyon oficial

Este repositório é um fork de [veyon/veyon](https://github.com/veyon/veyon). Hoje
ele é idêntico ao upstream — nenhuma customização própria foi aplicada ainda.
Este documento define o processo para manter o fork atualizado com correções
(principalmente de segurança) do projeto original ao mesmo tempo em que
customizações locais são adicionadas, evitando divergência difícil de resolver.

## Modelo de branches

- **`main`**: espelha o `main` do upstream. Não recebe commits diretos com
  customizações — apenas merges/rebases vindos de `veyon/veyon`. Isso mantém
  um caminho de atualização sempre limpo.
- **`etisa/main`** (criar quando a primeira customização for necessária):
  branch de trabalho onde vivem as customizações específicas da ETISA. É
  baseada em `main` e recebe as mudanças locais via PRs normais.

Fluxo:

```
veyon/veyon (upstream) --> main (espelho) --> etisa/main (customizações)
```

## Configurando o remote do upstream

```bash
git remote add upstream https://github.com/veyon/veyon.git
git remote set-url --push upstream DISABLE
```

A segunda linha impede pushes acidentais para o repositório oficial.

## Sincronizando `main` com o upstream

```bash
git fetch upstream
git checkout main
git merge --ff-only upstream/main
git push origin main
```

Se `--ff-only` falhar, é sinal de que `main` recebeu commits locais — o que
não deveria acontecer neste modelo. Nesse caso, mova esses commits para
`etisa/main` antes de prosseguir.

## Trazendo as atualizações para o branch de customização

Depois de atualizar `main`, rebase (ou merge, conforme preferência da equipe)
o branch de customizações sobre ele:

```bash
git checkout etisa/main
git rebase main
# resolver conflitos, se houver
git push --force-with-lease origin etisa/main
```

Rebase é recomendado enquanto o branch de customizações não for
compartilhado amplamente; se vários desenvolvedores trabalham nele
simultaneamente, prefira `git merge main` para não reescrever histórico
compartilhado.

## Periodicidade recomendada

- **Sincronização de `main`**: semanal, ou imediatamente após qualquer aviso
  de segurança do Veyon (acompanhar `SECURITY.md` do upstream e a lista de
  releases).
- **Dependabot** (`.github/dependabot.yml`): já configurado neste repositório
  para verificar semanalmente atualizações de GitHub Actions, dos submódulos
  C/C++ de terceiros (`3rdparty/*`) e do SDK Python do plugin webapi.
- **CodeQL** (`.github/workflows/codeql.yml`): roda em todo push/PR para
  `main` e semanalmente, cobrindo o núcleo C/C++ e o SDK Python.

## Documentando divergências

Toda customização aplicada em `etisa/main` que altere comportamento deve ser
registrada em um changelog próprio (ex.: `docs/ETISA_CHANGES.md`, a ser
criado quando a primeira customização existir), citando o commit e o motivo.
Isso facilita tanto auditorias quanto a resolução de conflitos em futuros
rebases.
