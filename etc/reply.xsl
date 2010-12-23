<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:output method="html"/>


<xsl:template match="/reply">
<HTML>
<HEAD>
<TITLE><xsl:value-of select="status"/></TITLE>
</HEAD>
<BODY>
<H1>
       <xsl:value-of select="status"/>
</H1>
       <xsl:choose>
       <xsl:when test="message/moduleinfo/*">
		<xsl:apply-templates select="message/moduleinfo"/>
       </xsl:when>
       <xsl:when test="message/meterinfos/*">
		<xsl:apply-templates select="message/meterinfos"/>
       </xsl:when>
       <xsl:otherwise> 
       		<xsl:apply-templates select="message"/>
       </xsl:otherwise>
       </xsl:choose>
</BODY>
</HTML>
</xsl:template>


<!-- template for module info -->

<xsl:template match="message/moduleinfo">
<H2>
        Module: <xsl:value-of select="module/name"/>
</H2>
<B>     (version <xsl:value-of select="module/version"/>
        , uid=<xsl:value-of select="module/uid"/>)</B>

<P>Creation Date: <xsl:value-of select="module/created"/><br>
   Last Modified: <xsl:value-of select="module/modified"/></br></P>

<B>Description:</B>

<P><TABLE>
<TR><TD width="150">Short</TD><TD><xsl:value-of select="module/brief"/></TD></TR>
<TR><TD>Verbose</TD><TD><xsl:value-of select="module/verbose"/></TD></TR>
<TR><TD>documented at</TD><TD><xsl:value-of select="module/htmldoc"/></TD></TR>
<TR><TD>per Rule paramaters</TD><TD><xsl:value-of select="module/parameters"/></TD></TR>
<TR><TD>Exported Data</TD><TD><xsl:value-of select="module/results"/></TD></TR>
</TABLE></P>

<B>Author:</B>

<P><TABLE>
<TR><TD width="150">Name</TD><TD><xsl:value-of select="author/name"/></TD></TR>
<TR><TD>Affiliation</TD><TD><xsl:value-of select="author/affiliation"/></TD></TR>
<TR><TD>E-Mail</TD>
<TD><xsl:element name="a">
        <xsl:attribute name="href">
                mailto:<xsl:value-of select="author/email"/>
        </xsl:attribute>
        <xsl:value-of select="author/email"/>
</xsl:element>
</TD></TR>
<TR><TD>Homepage</TD>
<TD><xsl:element name="a">
       <xsl:attribute name="href">
                <xsl:value-of select="author/homepage"/>
        </xsl:attribute>
        <xsl:value-of select="author/homepage"/>
</xsl:element>
</TD></TR>
</TABLE></P>
</xsl:template>


<!-- template for meter infos -->

<xsl:template match="message/meterinfos">

<TABLE>
<TR>
<TH>Item</TH>
<TH>Value</TH>
</TR>
<xsl:for-each select="//info">
	<TR>
	<TD VALIGN="top">
		<xsl:value-of select="@name"/>
	</TD>
	<TD>
		<xsl:call-template name="replace-nl"/>
	</TD>
	</TR>
</xsl:for-each>
</TABLE>    

</xsl:template>



<!-- template for everything else -->

<xsl:template match="message">
<P><PRE>
      <xsl:value-of select="."/>
</PRE></P>
</xsl:template>



<!-- (recursive) template for converting \n to <br> in a string -->

<xsl:template name="replace-nl">
    <xsl:param name="string" select="."/>

    <xsl:choose>
        <xsl:when test="contains($string, '&#10;')">
            <xsl:value-of select="substring-before($string, '&#10;')"/>
            <br/>
            <xsl:call-template name="replace-nl">
                <xsl:with-param name="string" select="substring-after($string, '&#10;')"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
	    <xsl:value-of select="$string"/>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>


</xsl:stylesheet>
