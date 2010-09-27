----------------------------------------------------------------------
TODO: write how to build, run, profile
----------------------------------------------------------------------

Some ant targets:

    <target name="default" depends="test,jar,javadoc" description="Build and test whole project."/>

    <target name="compile" depends="init,deps-jar,-verify-automatic-build,-pre-pre-compile,-pre-compile,-do-compile,-post-compile" description="Compile project."/>

    <target name="run" depends="init,compile" description="Run a main class.">

    <target name="clean" depends="init,deps-clean,-do-clean,-post-clean" description="Clean build products."/>

    <target name="jar" depends="init,compile,-pre-jar,-do-jar-with-manifest,-do-jar-without-manifest,-do-jar-with-mainclass,-do-jar-with-libraries-and-splashscreen,-do-jar-with-libraries,-post-jar" description="Build JAR."/>

    <target name="javadoc" depends="init,-javadoc-build,-javadoc-browse" description="Build Javadoc."/>

    <target name="deps-jar" depends="init,-deps-jar-init" unless="no.deps">

    <target name="deps-clean" depends="init,-deps-clean-init" unless="no.deps">
----------------------------------------------------------------------
